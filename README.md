# WebSocket Server

## Description

This project implements a WebSocket server that allows clients to subscribe to topics and send/receive messages. It supports configuration via file and command line, and logs activities to a log file or syslog.

## Prerequisites

- g++
- libwebsocketpp-dev
- libboost-all-dev
- libssl-dev

## Installation on Ubuntu and Related Distributions

1. Install the required dependencies:

```bash
sudo apt install g++ libwebsocketpp-dev libboost-all-dev libssl-dev
```
2. Clone the repository:

```bash
git clone https://github.com/clementefeo/websockets-file-sender.git
cd websockets-file-sender
```
3. Compile the server:

```bash
g++ -std=c++11 -o websocket_server websocket_server.cpp -I ./websocketpp -lboost_system -lboost_thread -lssl -lcrypto
```
## Usage

Run the server with:

```bash
./websocket_server [options]
```

Command-line Options

- -p <port>: Specify the port to use (default: 8765)
- -i <IP>: Specify the IP address to listen on (default: all IPs)
- -s <max size in MB>: Specify the maximum message size in MB (default: 200 MB)
- -l <log file path>: Specify the path for the log file (default: syslog)
- -c <config file>: Specify a custom configuration file
- -h: Show help information

## Setting up as a System Service

1. Create a service file:

```bash
sudo nano /etc/init.d/websocket_server
```
2. Copy the following content into the file:

```bash
#!/bin/bash
# /etc/init.d/websocket_server

### BEGIN INIT INFO
# Provides:          websocket_server
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: WebSocket Server
# Description:       Starts the WebSocket server
### END INIT INFO

DIR=/usr/local/bin/websocket_server
DAEMON=$DIR/websocket_server
DAEMON_NAME=websocket_server

# Add any command line options for your daemon here
DAEMON_OPTS=""

# This next line determines what user the script runs as.
DAEMON_USER=root

# The process ID of the script when it runs is stored here:
PIDFILE=/var/run/$DAEMON_NAME.pid

. /lib/lsb/init-functions

do_start() {
    log_daemon_msg "Starting system $DAEMON_NAME daemon"
    start-stop-daemon --start --background --pidfile $PIDFILE --make-pidfile --user $DAEMON_USER --exec $DAEMON -- $DAEMON_OPTS
    log_end_msg $?
}
do_stop() {
    log_daemon_msg "Stopping system $DAEMON_NAME daemon"
    start-stop-daemon --stop --pidfile $PIDFILE --retry 10
    log_end_msg $?
}

case "$1" in

    start|stop)
        do_${1}
        ;;
    restart|reload|force-reload)
        do_stop
        do_start
        ;;
    status)
        status_of_proc "$DAEMON_NAME" "$DAEMON" && exit 0 || exit $?
        ;;
    *)
        echo "Usage: /etc/init.d/$DAEMON_NAME {start|stop|restart|status}"
        exit 1
        ;;
esac
exit 0
```

3. Make the service file executable:

```bash
sudo chmod +x /etc/init.d/websocket_server
```

4. Move the compiled server to the appropriate directory:

```bash
sudo mkdir -p /usr/local/bin/websocket_server
sudo mv websocket_server /usr/local/bin/websocket_server/
```

5. Create a configuration File

Create a configuration file at `/etc/websocket_server/websocket_server.conf` with the following content:

```bash
sudo mkdir -p /etc/websocket_server
sudo nano /etc/websocket_server/websocket_server.conf
```

Add the following example content to the file:

```bash
# WebSocket Server Configuration

# Port to listen on
port=8765

# IP address to bind to (use 0.0.0.0 for all interfaces)
ip=0.0.0.0

# Maximum message size in MB
max_message_size=200

# Log file path (leave empty to use syslog)
log_file=/var/log/websocket_server.log

```

6. Register the script in the default run levels:

```bash
sudo update-rc.d websocket_server defaults
```

7. Start the service:

```bash
sudo service websocket_server start
```
8. Check the status of the service:

```bash
sudo service websocket_server status
```

## Contributing
Please read CONTRIBUTING.md for details on our code of conduct, and the process for submitting pull requests to us.

## License
This project is licensed under the MIT License - see the LICENSE.md file for details.
