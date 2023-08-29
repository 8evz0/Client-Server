# FSP2
Client-server application.

Server:
sudo ./lab2lvcN32451_server -h

    Usage: server [options]
    Options:
      -w N              Set the waiting time in seconds
      -d                Run as a daemon
      -l /path/to/log   Set the path to the log file
      -a ip             Set the server address
      -p port           Set the server port
      -v                Show program version
      -h                Show this help message
      -с                Operation of the server in 'chat' mode to exchange messages (data) between all clients connected to the server
      -b                Client management mode for remote program launch (full access to the shell is not available, so it is possible to make mistakes using standard Unix-system commands: cd, netstat etc.). 
      -D ip             Client management mode to send from clients packets to the choosed address 
      -T <soon :(>      Command shell management
    Environment variables:
      LAB2WAIT           Imulate processing delay (in seconds)
      LAB2LOGFILE        Path to log file
      LAB2ADDR           Server address
      LAB2PORT           Server port
      LAB2DEBUG          Enable debug mode

Client: 
sudo ./lab2lvcN32451_client -h

    Usage: server [options]
    Options:
      -a ip             Set the server address
      -p port           Set the server port
      -v                Show program version
      -h                Show this help message
      -с                Operation of the server in 'chat' mode to exchange messages (data) between all clients connected to the server
      -b                Data transmission to the server from host
      -D                Send packets of data to choosed address
      -d                Daemon mode for client P.S. add to autostart
      -T <soon :(>      Command shell management
    Environment variables:
      LAB2ADDR           Server address
      LAB2PORT           Server port
      LAB2DEBUG          Enable debug mode
