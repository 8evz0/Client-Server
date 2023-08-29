/*
*Просмотр акт. соед. netstat -tuln или ss -tuln
*Закрытие соед. sudo tcpkill host <адрес_удаленного_хоста> and port <порт> или sudo tcpdrop <адрес_удаленного_хоста> <порт>
* sudo lsof -i :<порт> или sudo kill -9 <PID>
*/

/*
Перспективы развития:
  1. Шифрование сообщений + использование SSL/TLS
  2. Расширить функционал -T (управление командной оболочкой)
  3. !!! Сервер и клиент запускается только с плагинами (без запуска с плагинами ни одна из программ не работает) или уст. соед. и пред. выб. польз. какой плагин нужно подкл. (плагины - модули сервера или клиента (сейчас это опции и соот. функции  в коде))
  4. Плагин "фильтр-пакетов на основе интерфейса netfilter"
  5. ...
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/time.h>

#define MAX_BUFFER_SIZE 65538 // Максимальный размер буфера (65536 байтов данных + 2 байта на размер)
#define ERROR_RESPONSE 0xFF   // Код ошибки в ответе
#define DATA_RESPONSE 0x00    // Код успешного ответа

#define DEFAULT_WAIT_TIME 0
#define DEFAULT_LOG_FILE "/tmp/lab2.log" 
#define DEFAULT_ADDR "127.0.0.1"
#define DEFAULT_PORT 12345
#define MAX_BUFFER_SIZE 65538

// Указатель на лог-файл
FILE *log_file;

// Глобальные переменные для параметров main 
int wait_time = DEFAULT_WAIT_TIME;
int daemon_mode = 0;
char log_path[PATH_MAX] = DEFAULT_LOG_FILE;
char server_addr[16] = DEFAULT_ADDR; // Update the default value here
int server_port = DEFAULT_PORT;
int show_version = 0;
int show_help = 0;
int debug_mode = 0;
int bots_mode = 0;
int chat_mode = 0;
int d_mode = 0;

// Глобальные переменные для опции -D
char dest_addr_opt_D[16] = DEFAULT_ADDR; 

// Для режима ботов
//#define MAX_CLIENTS 1000000 // Maximum number of clients

// Глобальные переменные для опции -c
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
int client4chat_socket;
typedef struct {
    int sockfd;
    char username[50];
} Client;

Client clients_c[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex_c = PTHREAD_MUTEX_INITIALIZER;

// Глобальные переменные для опции -b
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int clients[MAX_CLIENTS];
int num_clients = 0;
char broadcast_msg[MAX_BUFFER_SIZE];
char result_message[MAX_BUFFER_SIZE];

// Глобальные переменные для опции -D
char client_ip[INET6_ADDRSTRLEN]; // Достаточный размер для IPv4 и IPv6
void *client_addr_ptr;
int client_port;

// Глобальные переменные для хранения статистистки, которая вызывается при получение сигналов SIGTERM,SIGUSR1,SIG
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
int success_requests = 0;
int error_requests = 0;
time_t start_time;

struct timeval start,end;

float time_diff(struct timeval *start,struct timeval *end)
{
return (end->tv_sec-start->tv_sec)+1e-6*(end->tv_usec-start->tv_usec);
}

void log_error(const char *message)
{
    time_t current_time;
    time(&current_time);
    
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%d.%m.%y %H:%M:%S", localtime(&current_time));
    
    pthread_mutex_lock(&stats_mutex);
    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fflush(log_file); // Flush log file buffer
    pthread_mutex_unlock(&stats_mutex);
}

void sigusr1_handler(int signum, siginfo_t *info, void *context) 
{
    (void)signum; 
    (void)info;
    (void)context;
    
    time_t current_time;
    time(&current_time);
    
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%d.%m.%y %H:%M:%S", localtime(&current_time));
    
    pthread_mutex_lock(&stats_mutex);
    gettimeofday(&end, NULL);
    fprintf(log_file, "\n------ Server Statistics ------\n");
    fprintf(log_file, "Time: [%s] \n", timestamp);
    fflush(log_file); // Flush log file buffer
    fprintf(log_file, "Elapsed time: %0.8f sec\n", time_diff(&start,&end));
    fprintf(log_file, "Successful requests: %d\n", success_requests);
    fprintf(log_file, "Error requests: %d\n", error_requests);
    fprintf(log_file, "\n################################\n");
    fprintf(stderr, "\n------ Server Statistics ------\n");
    fprintf(stderr, "Time: [%s] \n", timestamp);
    fflush(stderr); 
    fprintf(stderr, "Elapsed time: %0.8f sec\n", time_diff(&start,&end));
    fprintf(stderr, "Successful requests: %d\n", success_requests);
    fprintf(stderr, "Error requests: %d\n", error_requests);
    fprintf(stderr, "\n################################\n");
    pthread_mutex_unlock(&stats_mutex);
}

void termination_handler(int signum) 
{
    (void)signum; 
    switch (signum) 
    {
        case SIGINT:
            kill(getpid(), SIGUSR1);
            break;
        case SIGTERM:
            kill(getpid(), SIGUSR1);
            break;
        case SIGQUIT:
            kill(getpid(), SIGUSR1);
            break;
        case SIGTSTP:
            kill(getpid(), SIGUSR1);
            break;
        default:
            break;
    }
    
    exit(0);
}

void print_help() 
{
    fprintf(stdout,"Usage: server [options]\n");
    fprintf(stdout,"Options:\n");
    fprintf(stdout,"  -w N              Set the waiting time in seconds\n");
    fprintf(stdout,"  -d                Run as a daemon\n");
    fprintf(stdout,"  -l /path/to/log   Set the path to the log file\n");
    fprintf(stdout,"  -a ip             Set the server address\n");
    fprintf(stdout,"  -p port           Set the server port\n");
    fprintf(stdout,"  -v                Show program version\n");
    fprintf(stdout,"  -h                Show this help message\n");
    fprintf(stdout,"  -с                Operation of the server in 'chat' mode to exchange messages (data) between all clients connected to the server\n");
    fprintf(stdout,"  -b                Client management mode for remote program launch (full access to the shell is not available, so it is possible to make mistakes using standard Unix-system commands: cd, netstat etc.). \n");
    fprintf(stdout,"  -D ip             Client management mode to send from clients packets to the choosed address \n");
    fprintf(stdout,"  -T <soon :(>      Command shell management\n");
    fprintf(stdout,"Environment variables:\n");
    fprintf(stdout,"  LAB2WAIT           Imulate processing delay (in seconds)\n");
    fprintf(stdout,"  LAB2LOGFILE        Path to log file\n");
    fprintf(stdout,"  LAB2ADDR           Server address\n");
    fprintf(stdout,"  LAB2PORT           Server port\n");
    fprintf(stdout,"  LAB2DEBUG          Enable debug mode\n");
}

void *handler_chat(void *arg)
{
  int client_sockfd = *(int *)arg;
    int client_index = -1;

    char username[50];
    recv(client_sockfd, username, sizeof(username), 0);

    pthread_mutex_lock(&clients_mutex_c);
    if (client_count < MAX_CLIENTS) 
    {
        clients_c[client_count].sockfd = client_sockfd;
        strcpy(clients_c[client_count].username, username);
        client_index = client_count;
        client_count++;
    }
    pthread_mutex_unlock(&clients_mutex_c);

    if (client_index == -1)
    {
        fprintf(stdout,"Client rejected: Maximum number of clients reached.\n");
        fprintf(log_file,"Client rejected: Maximum number of clients reached.\n");
        close(client_sockfd);
        return NULL;
    }

    fprintf(stdout,"Client connected: %s\n", clients_c[client_index].username);
    fprintf(log_file,"Client connected: %s\n", clients_c[client_index].username);

    char join_message[100];
    sprintf(join_message, "%s has joined the chat.\n", clients_c[client_index].username);
    
    pthread_mutex_lock(&clients_mutex_c);
    for (int i = 0; i < client_count; i++) 
    {
        if (i != client_index) 
        {
            send(clients_c[i].sockfd, join_message, strlen(join_message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex_c);

    while (1) 
    {
        char client_message[BUFFER_SIZE];
        ssize_t bytes_received = recv(client_sockfd, client_message, sizeof(client_message), 0);
        if (bytes_received <= 0) 
        {
            break;
        }

        char broadcast_msg[BUFFER_SIZE + 51];
        sprintf(broadcast_msg, "%s: %s",clients_c[client_index].username, client_message);

        pthread_mutex_lock(&clients_mutex_c);
        for (int i = 0; i < client_count; i++)
        {
            if (i != client_index) 
            {
                send(clients_c[i].sockfd, broadcast_msg, strlen(broadcast_msg), 0);
            }
        }
        pthread_mutex_unlock(&clients_mutex_c);
    }

    fprintf(stdout,"Client disconnected: %s\n", clients_c[client_index].username);
    fprintf(log_file,"Client disconnected: %s\n", clients_c[client_index].username);
    
    pthread_mutex_lock(&clients_mutex_c);
    for (int i = client_index; i < client_count - 1; i++)
    {
        clients_c[i] = clients_c[i + 1];
    }
    client_count--;
    pthread_mutex_unlock(&clients_mutex_c);

    close(client_sockfd);

    return NULL;
}

void *botnet_handler(void *arg)
{
  (void)arg;

  for (int i = 0; i < num_clients; i++) 
  {
    send(clients[i], dest_addr_opt_D, strlen(dest_addr_opt_D), 0);
  }

  return NULL;
}

void *broadcast_handler(void *arg) 
{
    (void)arg;
    
    char input[MAX_BUFFER_SIZE];
    
    fprintf(stdout,"Enter message to broadcast('q' to quit): ");
    fflush(stdout); // Flush log file buffer
    fgets(input, sizeof(input), stdin);
    if(input[0]=='q')
    {
      exit(1);
    }

    while (1) 
    {
        strcpy(broadcast_msg, input);
        for (int i = 0; i < num_clients; i++) 
        {
            send(clients[i], broadcast_msg, strlen(broadcast_msg), 0);
        }
        
        
        pthread_mutex_lock(&clients_mutex);

        for (int i = 0; i < num_clients; i++) 
        {

              size_t message_length;
              ssize_t bytes_received = recv(clients[i], &message_length, sizeof(message_length), 0);
              if (bytes_received <= 0) 
              {
                  perror("Error receiving message length");
                  log_error("Error receiving message length");
                  break;
              }

              char received_data[MAX_BUFFER_SIZE];
              bytes_received = recv(clients[i], received_data, message_length, 0);
              if (bytes_received <= 0) 
              {
                  perror("Error receiving data");
                  log_error("Error receiving data");
                  break;
              }
 
              time_t current_time;
              time(&current_time);
              
              char timestamp[20];
              strftime(timestamp, sizeof(timestamp), "%d.%m.%y %H:%M:%S", localtime(&current_time));
              
              fprintf(stderr,"INFO: Received data from client %s:%d [%s]\n %.*s", client_ip, client_port, timestamp,(int)message_length, received_data);
              fprintf(log_file,"INFO: Received data from client %s:%d [%s]\n %.*s", client_ip, client_port, timestamp,(int)message_length, received_data);
              
        }
        pthread_mutex_unlock(&clients_mutex);
        
        fprintf(stdout,"Enter message to broadcast('q' to quit): ");
        fflush(stdout);
        fgets(input, sizeof(input), stdin);
        if(input[0]=='q')
        {
          return NULL;
        }

    }
   
   return NULL;
}


// Функция для вычисления CRC-16-ANSI
uint16_t calculate_crc16(const uint8_t *data, size_t size) 
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < size; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) 
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// Обработчик клиентского запроса
void *client_handler(void *arg) 
{

    int client_socket = *((int *)arg);
    uint8_t buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_received;


    bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (debug_mode) 
    {
        fprintf(log_file, "\tData received: %ld\n", bytes_received);
        fprintf(stdout, "\tData received: %ld\n", bytes_received);

        if (bytes_received > 0) 
        {
            fprintf(log_file, "\tReceived data: ");
            char received_data_string[3 * (size_t)bytes_received + 1]; 

            for (size_t i = 0; i < (size_t)bytes_received; i++) 
            {
                fprintf(log_file, "%02x ", buffer[i]);
                snprintf(received_data_string + 3 * i, 4, "%02x ", buffer[i]);
            }
            received_data_string[3 * (size_t)bytes_received] = '\0'; 

            fprintf(log_file, "\n");

            fprintf(stdout, "\tReceived data: ");
            
            for (size_t i = 0; i < (size_t)bytes_received; i++) 
            {
                fprintf(stdout, "%02x ", buffer[i]);
            }
            fprintf(stdout, "\n");

            fprintf(log_file, "\tReceived data as string: %s\n", received_data_string); 
        }
    }
    else
    {
        fprintf(log_file, "\tData received: %ld\n", bytes_received);
        fprintf(stdout, "\tData received: %ld\n", bytes_received);

        if (bytes_received > 0) 
        {
            fprintf(log_file, "\tReceived data: ");
            char received_data_string[3 * (size_t)bytes_received + 1]; 

            for (size_t i = 0; i < (size_t)bytes_received; i++) 
            {
                fprintf(log_file, "%02x ", buffer[i]);
                snprintf(received_data_string + 3 * i, 4, "%02x ", buffer[i]);
            }
            received_data_string[3 * (size_t)bytes_received] = '\0'; 

            fprintf(log_file, "\n");

            fprintf(stdout, "\tReceived data: ");
            for (size_t i = 0; i < (size_t)bytes_received; i++) 
            {
                fprintf(stdout, "%02x ", buffer[i]);
            }
            fprintf(stdout, "\n");

            fprintf(log_file, "\tReceived data as string: %s\n", received_data_string); 
        }
    }
    if (bytes_received > 0) 
    {
      char received_data_string[3 * (size_t)bytes_received + 1]; 

      for (size_t i = 0; i < (size_t)bytes_received; i++) 
      {
        
        snprintf(received_data_string + 3 * i, 4, "%02x ", buffer[i]);
      }
      received_data_string[3 * (size_t)bytes_received] = '\0'; 
      
      /*
       На будущее - это данные внутри пакета
            received_data_string
      */
    }
    
    if (bytes_received < 0) 
    {
        perror("Error receiving data from client");
        log_error("Error receiving data from client");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Получение размера данных из первых двух байтов
    uint16_t data_size = (buffer[0] << 8) | buffer[1];

    if (bytes_received != data_size + 2) 
    {
        // Ошибка в запросе
        pthread_mutex_lock(&stats_mutex);
        error_requests++;
        pthread_mutex_unlock(&stats_mutex);

        uint8_t error_response[3] = {ERROR_RESPONSE, 0x00, 0x00};
        send(client_socket, error_response, sizeof(error_response), 0);
    } else 
    {
        // Корректный запрос - вычисление контрольной суммы и отправка ответа
        uint16_t crc = calculate_crc16(buffer + 2, data_size);
        uint8_t data_response[3] = {DATA_RESPONSE, (crc >> 8) & 0xFF, crc & 0xFF};

        pthread_mutex_lock(&stats_mutex);
        success_requests++;
        pthread_mutex_unlock(&stats_mutex);

        send(client_socket, data_response, sizeof(data_response), 0);
    }

    
    if (debug_mode) 
    {
        pthread_mutex_lock(&stats_mutex);
        
        time_t current_time;
        time(&current_time);
        
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%d.%m.%y %H:%M:%S", localtime(&current_time));

        fprintf(stdout, "INFO:  connection closed whith %s:%d [%s] \n", client_ip, client_port, timestamp); 
        
        fprintf(log_file, "INFO:  connection closed whith %s:%d [%s] \n", client_ip, client_port, timestamp);
        
        fflush(stdout);  
        fflush(log_file); 
        pthread_mutex_unlock(&stats_mutex);
    }
    else
    {
        pthread_mutex_lock(&stats_mutex);
        
        time_t current_time;
        time(&current_time);
        
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%d.%m.%y %H:%M:%S", localtime(&current_time));

        fprintf(stdout, "INFO:  connection closed whith %s:%d [%s] \n", client_ip, client_port, timestamp); 
        
        fprintf(log_file, "INFO:  connection closed whith %s:%d [%s] \n", client_ip, client_port, timestamp);
        
        fflush(stdout);  
        fflush(log_file); 
        
        pthread_mutex_unlock(&stats_mutex);
    }
    
    close(client_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) 
{
  gettimeofday(&start, NULL);

  struct sigaction sa_usr1, sa_term;
  sa_usr1.sa_flags = SA_SIGINFO;
  sa_usr1.sa_sigaction = sigusr1_handler;
  sigemptyset(&sa_usr1.sa_mask);
  sigaction(SIGUSR1, &sa_usr1, NULL);

  sa_term.sa_handler = termination_handler;
  sigemptyset(&sa_term.sa_mask);
  sa_term.sa_flags = 0;
  sigaction(SIGINT, &sa_term, NULL);
  sigaction(SIGTERM, &sa_term, NULL);
  sigaction(SIGQUIT, &sa_term, NULL);
  sigaction(SIGTSTP, &sa_term, NULL);
  
  const char *wait_time_env = getenv("LAB2WAIT");
  if (wait_time_env) 
  {
    wait_time = atoi(wait_time_env);
  }

  const char *log_file_env = getenv("LAB2LOGFILE");
  if (log_file_env) 
  {
    strncpy(log_path, log_file_env, sizeof(log_path) - 1);
    log_path[sizeof(log_path) - 1] = '\0';
  }

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
  
  log_file = fopen(DEFAULT_LOG_FILE, "a+");
  if (!log_file) 
  {
    perror("Error opening log file");
    log_error("Error opening log file");
    return 1;
  }
    
  if(debug_mode)
  {
    time_t current_time;
    time(&current_time);
    
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%d.%m.%y %H:%M:%S", localtime(&current_time));

    fprintf(log_file, "\n################################\n");
    fprintf(log_file, "Server run at [%s] \n", timestamp);
    fflush(log_file); 
    
    fprintf(stdout, "\n################################\n");
    fprintf(stdout, "Server run at  [%s] \n", timestamp);
    fflush(stdout); 
  }

  int opt;
  while ((opt = getopt(argc, argv, "w:dl:a:p:vhcbD:")) != -1) 
  {
    switch (opt) 
    {
      case 'D':
        d_mode=1;
        strncpy(dest_addr_opt_D, optarg, sizeof(dest_addr_opt_D));
        dest_addr_opt_D[sizeof(dest_addr_opt_D) - 1] = '\0';
        break;
      case 'c':
        chat_mode = 1; 

        break;
      case 'b':
        bots_mode = 1; 
        break;
      case 'w':
        wait_time = atoi(optarg);
        if (debug_mode) 
        {
          fprintf(log_file, "DEBUG: wait_time = %d\n",wait_time);
        }
        break;
      case 'd':
        daemon_mode = 1;
        if (debug_mode) 
        {
          fprintf(log_file, "DEBUG: daemon_mode = %d\n",daemon_mode);
        }
        break;
      case 'l':
        strncpy(log_path, optarg, sizeof(log_path));
        log_path[sizeof(log_path) - 1] = '\0';
        if (debug_mode) 
        {
          fprintf(log_file, "DEBUG: log_path = %s\n",log_path);
        }
        break;
      case 'a':
        strncpy(server_addr, optarg, sizeof(server_addr));
        server_addr[sizeof(server_addr) - 1] = '\0';
        if (debug_mode) 
        {
          fprintf(log_file, "DEBUG: server_addr = %s\n",server_addr);
        }
        break;
      case 'p':
        server_port = atoi(optarg);
        if (debug_mode) 
        {
          fprintf(log_file, "DEBUG: server_port = %d\n",server_port);
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

  if (debug_mode) 
  {
    
    fprintf(log_file, "DEBUG: log_path = %s\n",log_path);
    fprintf(log_file, "DEBUG: server_addr = %s\n",server_addr);
    fprintf(log_file, "DEBUG: server_port = %d\n",server_port);
    fprintf(log_file, "DEBUG: daemon_mode = %d\n",daemon_mode);
    fprintf(log_file, "DEBUG: bots_mode = %d\n", bots_mode);
    fprintf(log_file, "DEBUG: chat_mode = %d\n", chat_mode);
    fprintf(log_file, "DEBUG: wait_time = %d\n",wait_time);
  
    fprintf(stdout, "DEBUG: log_path = %s\n",log_path);
    fprintf(stdout, "DEBUG: server_addr = %s\n",server_addr);
    fprintf(stdout, "DEBUG: server_port = %d\n",server_port);
    fprintf(stdout, "DEBUG: daemon_mode = %d\n",daemon_mode);
    fprintf(stdout, "DEBUG: bots_mode = %d\n", bots_mode);
    fprintf(stdout, "DEBUG: chat_mode = %d\n", chat_mode);
    fprintf(stdout, "DEBUG: wait_time = %d\n",wait_time);

  }
  
  
  // Если указан режим демона, выполнить его
  if (daemon_mode) 
  {
      if (setsid() < 0)
      {
        perror("Error creating new session");
        log_error("Error creating new session");
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
          log_error("Error changing working directory");
          exit(EXIT_FAILURE);
      }
  }
    
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) 
  {
      log_error("Error creating socket");
      perror("Error creating socket");
      return EXIT_FAILURE;
  }
  else
  {
    if(debug_mode)
    {
          fprintf(log_file, "DEBUG: socket created\n");
          fprintf(stdout, "DEBUG: socket created\n");
    }
  }

  struct sockaddr_in server_addr;
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY; 
  
  server_addr.sin_port = htons(server_port); 

  if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
  {
    log_error("Error binding socket");
    perror("Error binding socket");
    close(server_socket);
    return EXIT_FAILURE;
  }
  else
  {
    if(debug_mode)
    {
          fprintf(log_file, "DEBUG: success binding\n");
          fprintf(stdout, "DEBUG: success binding \n");
    }
  }

  if (listen(server_socket, 10) < 0)
  {
    log_error("Error listening on socket");
    perror("Error listening on socket");
    close(server_socket);
    return EXIT_FAILURE;
  }
  else
  {
    if(debug_mode)
    {
          fprintf(log_file, "DEBUG: listening port\n");
          fprintf(stdout, "DEBUG: listening port \n");
    }
  }
  
    
    if (bots_mode) 
    {
      
      while (1) 
      {
          int client4broadcast_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
          if (client4broadcast_socket < 0) 
          {
              log_error("Error accepting connection");
              perror("Error accepting connection");
              continue;
          }
          else
          {
            if(debug_mode)
            {
                  fprintf(log_file, "DEBUG: connection accepted\n");
                  fprintf(stdout, "DEBUG: connection accepted\n");
            }
          }
          
          if (client_addr.sin_family == AF_INET) 
          {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
            client_addr_ptr = &(ipv4->sin_addr);
          } 
          else 
          {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
            client_addr_ptr = &(ipv6->sin6_addr);
          }


          inet_ntop(client_addr.sin_family, client_addr_ptr, client_ip, sizeof(client_ip));

          if (client_addr.sin_family == AF_INET) 
          {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
            client_port = ntohs(ipv4->sin_port);
          } 
          else 
          {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
            client_port = ntohs(ipv6->sin6_port);
          }
          
          if (debug_mode) 
          {
            pthread_mutex_lock(&stats_mutex);
            
            time_t current_time;
            time(&current_time);
            
            char timestamp[20];
            strftime(timestamp, sizeof(timestamp), "%d.%m.%y %H:%M:%S", localtime(&current_time));

            fprintf(stdout, "INFO:   new connection from %s:%d\n", client_ip, client_port);
            fprintf(stdout, "\tTime: [%s] \n", timestamp);
            
            fprintf(log_file, "INFO:   new connection from %s:%d\n", client_ip, client_port);
            fprintf(log_file, "\tTime: [%s] \n", timestamp);
            
            fflush(stdout);  
            fflush(log_file); 
            pthread_mutex_unlock(&stats_mutex);
          }
          else
          {
            pthread_mutex_lock(&stats_mutex);
            
            time_t current_time;
            time(&current_time);
            
            fprintf(stdout, "INFO:    new connection from %s:%d\n", client_ip, client_port);
            fprintf(stdout, "\tTime: %s", ctime(&current_time));
            
            fprintf(log_file, "INFO:   new connection from %s:%d\n", client_ip, client_port);
            fprintf(log_file, "\tTime: %s", ctime(&current_time));
            
            fflush(stdout);  
            fflush(log_file); 
            pthread_mutex_unlock(&stats_mutex);
          }
          
          pthread_mutex_lock(&clients_mutex);
          clients[num_clients++] = client4broadcast_socket;
          pthread_mutex_unlock(&clients_mutex);
         

          pthread_t broadcast_thread_opt_b;
          int *broadcast_thread_opt_b_sock_ptr = malloc(sizeof(int));
          if (broadcast_thread_opt_b_sock_ptr == NULL) 
          {
            log_error("Error allocating memory for client_sock_ptr");
            perror("Error allocating memory for client_sock_ptr");
            free(broadcast_thread_opt_b_sock_ptr);
            close(client4broadcast_socket);
          }
          *broadcast_thread_opt_b_sock_ptr = client4broadcast_socket;
          
          pthread_create(&broadcast_thread_opt_b, NULL, broadcast_handler, (void *)broadcast_thread_opt_b_sock_ptr);
          pthread_detach(broadcast_thread_opt_b);
          

      }
      
      return 0;
  }
  else if (d_mode)
  {
    while (1) 
      {
          int client4d_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
          if (client4d_socket < 0) 
          {
              log_error("Error accepting connection");
              perror("Error accepting connection");
              continue;
          }
          else
          {
            if(debug_mode)
            {
                  fprintf(log_file, "DEBUG: connection accepted\n");
                  fprintf(stdout, "DEBUG: connection accepted\n");
            }
          }
          
          if (client_addr.sin_family == AF_INET) 
          {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
            client_addr_ptr = &(ipv4->sin_addr);
          } 
          else 
          {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
            client_addr_ptr = &(ipv6->sin6_addr);
          }

          inet_ntop(client_addr.sin_family, client_addr_ptr, client_ip, sizeof(client_ip));

          
          if (client_addr.sin_family == AF_INET) 
          {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
            client_port = ntohs(ipv4->sin_port);
          } 
          else 
          {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
            client_port = ntohs(ipv6->sin6_port);
          }
          
          if (debug_mode) 
          {
            pthread_mutex_lock(&stats_mutex);
            
            time_t current_time;
            time(&current_time);
            
            char timestamp[20];
            strftime(timestamp, sizeof(timestamp), "%d.%m.%y %H:%M:%S", localtime(&current_time));

            fprintf(stdout, "INFO:   new connection from %s:%d\n", client_ip, client_port);
            fprintf(stdout, "\tTime: [%s] \n", timestamp);
            
            fprintf(log_file, "INFO:   new connection from %s:%d\n", client_ip, client_port);
            fprintf(log_file, "\tTime: [%s] \n", timestamp);
            
            fflush(stdout);  
            fflush(log_file);
            pthread_mutex_unlock(&stats_mutex);
          }
          else
          {
            pthread_mutex_lock(&stats_mutex);
            
            time_t current_time;
            time(&current_time);
            
            fprintf(stdout, "INFO:    new connection from %s:%d\n", client_ip, client_port);
            fprintf(stdout, "\tTime: %s", ctime(&current_time));
            
            fprintf(log_file, "INFO:   new connection from %s:%d\n", client_ip, client_port);
            fprintf(log_file, "\tTime: %s", ctime(&current_time));
            
            fflush(stdout);  
            fflush(log_file); 
            pthread_mutex_unlock(&stats_mutex);
          }
          
          pthread_mutex_lock(&clients_mutex);
          clients[num_clients++] = client4d_socket;
          pthread_mutex_unlock(&clients_mutex);
         

          pthread_t d_thread_opt_d;
          int *d_thread_opt_d_sock_ptr = malloc(sizeof(int));
          if (d_thread_opt_d_sock_ptr == NULL) 
          {
            log_error("Error allocating memory for client_sock_ptr");
            perror("Error allocating memory for client_sock_ptr");
            free(d_thread_opt_d_sock_ptr);
            close(client4d_socket);
          }
          *d_thread_opt_d_sock_ptr = client4d_socket;
          
          pthread_create(&d_thread_opt_d, NULL, botnet_handler, (void *)d_thread_opt_d_sock_ptr);
          pthread_detach(d_thread_opt_d);
          

      }
  }
  else if (chat_mode)
  {
      while (1) 
      {
          client4chat_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
          if (client4chat_socket < 0) 
          {
              log_error("Error accepting connection");
              perror("Error accepting connection");
              continue;
          }
          else
          {
            if(debug_mode)
            {
                  fprintf(log_file, "DEBUG: connection accepted\n");
                  fprintf(stdout, "DEBUG: connection accepted\n");
            }
          }
          
          if (client_addr.sin_family == AF_INET) 
          {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
            client_addr_ptr = &(ipv4->sin_addr);
          } 
          else 
          {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
            client_addr_ptr = &(ipv6->sin6_addr);
          }

          inet_ntop(client_addr.sin_family, client_addr_ptr, client_ip, sizeof(client_ip));

          if (client_addr.sin_family == AF_INET) 
          {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
            client_port = ntohs(ipv4->sin_port);
          } 
          else 
          {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
            client_port = ntohs(ipv6->sin6_port);
          }
          
          if (debug_mode) 
          {
            pthread_mutex_lock(&stats_mutex);
            
            time_t current_time;
            time(&current_time);
            
            char timestamp[20];
            strftime(timestamp, sizeof(timestamp), "%d.%m.%y %H:%M:%S", localtime(&current_time));

            fprintf(stdout, "INFO:   new connection from %s:%d\n", client_ip, client_port);
            fprintf(stdout, "\tTime: [%s] \n", timestamp);
            
            fprintf(log_file, "INFO:   new connection from %s:%d\n", client_ip, client_port);
            fprintf(log_file, "\tTime: [%s] \n", timestamp);
            
            fflush(stdout);  
            fflush(log_file); 
            pthread_mutex_unlock(&stats_mutex);
          }
          else
          {
            pthread_mutex_lock(&stats_mutex);
            
            time_t current_time;
            time(&current_time);
            
            fprintf(stdout, "INFO:    new connection from %s:%d\n", client_ip, client_port);
            fprintf(stdout, "\tTime: %s", ctime(&current_time));
            
            fprintf(log_file, "INFO:   new connection from %s:%d\n", client_ip, client_port);
            fprintf(log_file, "\tTime: %s", ctime(&current_time));
            
            fflush(stdout);  
            fflush(log_file); 
            pthread_mutex_unlock(&stats_mutex);
          }
          
          pthread_mutex_lock(&clients_mutex);
          clients[num_clients++] = client4chat_socket;
          pthread_mutex_unlock(&clients_mutex);
         

          pthread_t c_thread_opt_c;
          int *c_thread_opt_c_sock_ptr = malloc(sizeof(int));
          if (c_thread_opt_c_sock_ptr == NULL) 
          {
            log_error("Error allocating memory for client_sock_ptr");
            perror("Error allocating memory for client_sock_ptr");
            free(c_thread_opt_c_sock_ptr);
            close(client4chat_socket);
          }
          *c_thread_opt_c_sock_ptr = client4chat_socket;
          
          pthread_create(&c_thread_opt_c, NULL, handler_chat, (void *)c_thread_opt_c_sock_ptr);
          pthread_detach(c_thread_opt_c);
      }
  }
  else
  {
    while (1) 
    {
      // Принятие входящего подключения
      int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
      if (client_socket < 0) 
      {
        log_error("Error accepting connection");
        perror("Error accepting connection");
        continue;
      }
      else
      {
        if(debug_mode)
        {
              fprintf(log_file, "DEBUG: connection accepted\n");
              fprintf(stdout, "DEBUG: connection accepted\n");
        }
      }


      if (client_addr.sin_family == AF_INET) 
      {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
        client_addr_ptr = &(ipv4->sin_addr);
      } 
      else 
      {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
        client_addr_ptr = &(ipv6->sin6_addr);
      }
      
      inet_ntop(client_addr.sin_family, client_addr_ptr, client_ip, sizeof(client_ip));
  
      if (client_addr.sin_family == AF_INET)
      {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
        client_port = ntohs(ipv4->sin_port);
      } 
      else 
      {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
        client_port = ntohs(ipv6->sin6_port);
      }
        
      if (debug_mode) 
      {
        pthread_mutex_lock(&stats_mutex);
        
        time_t current_time;
        time(&current_time);
        
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%d.%m.%y %H:%M:%S", localtime(&current_time));

        fprintf(stdout, "INFO:   new connection from %s:%d\n", client_ip, client_port);
        fprintf(stdout, "\tTime: [%s] \n", timestamp);
        
        fprintf(log_file, "INFO:   new connection from %s:%d\n", client_ip, client_port);
        fprintf(log_file, "\tTime: [%s] \n", timestamp);
        
        fflush(stdout);  
        fflush(log_file);
        pthread_mutex_unlock(&stats_mutex);
      }
      else
      {
        pthread_mutex_lock(&stats_mutex);
        
        time_t current_time;
        time(&current_time);
        
        fprintf(stdout, "INFO:    new connection from %s:%d\n", client_ip, client_port);
        fprintf(stdout, "\tTime: %s", ctime(&current_time));
        
        fprintf(log_file, "INFO:   new connection from %s:%d\n", client_ip, client_port);
        fprintf(log_file, "\tTime: %s", ctime(&current_time));
        
        fflush(stdout); 
        fflush(log_file);
        pthread_mutex_unlock(&stats_mutex);
      }

    
      pthread_t client_thread;
      int *client_sock_ptr = malloc(sizeof(int));
      if (client_sock_ptr == NULL) 
      {
        log_error("Error allocating memory for client_sock_ptr");
        perror("Error allocating memory for client_sock_ptr");
        free(client_sock_ptr);
        close(client_socket);
      }
      else
      {
        pthread_mutex_lock(&stats_mutex);
        if(wait_time)
        {
            for (int i = wait_time; i > 0; i--) 
            {
              fprintf(stdout, "\tWaiting");
              for (int j = 0; j < 3; j++) 
              {
                fprintf(stdout, ".");
              }
              fprintf(stdout, " %d", i);
              fflush(stdout); 
              sleep(1);
              if (i != 1) 
              {
                  fprintf(stdout, "\r"); 
              }
            }
        fprintf(stdout, "\n");
        }
        pthread_mutex_unlock(&stats_mutex);
      }
      *client_sock_ptr = client_socket;

      if (pthread_create(&client_thread, NULL, client_handler, (void *)client_sock_ptr) != 0) 
      {
        log_error("Error creating thread");
        perror("Error creating thread");
        free(client_sock_ptr);
        close(client_socket);
      }
      pthread_detach(client_thread);
      
      
    }
  }
  close(server_socket);
  fclose(log_file);
  gettimeofday(&end, NULL);
  return 0;

}
