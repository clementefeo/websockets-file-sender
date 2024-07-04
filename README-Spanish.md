# Servidor de Intercambio de Ficheros con WebSocket

## Descripción

Este proyecto implementa un Servidor de Intercambio de Ficheros con WebSocket que permite a los clientes suscribirse a temas e intercambiar archivos, que se reciben como mensajes. Soporta configuración a través de archivo y línea de comandos, y registra actividades en un archivo de registro o syslog.

## Prerrequisitos

- g++
- libwebsocketpp-dev
- libboost-all-dev
- libssl-dev

## Instalación en Ubuntu y Distribuciones Relacionadas

1. Instala las dependencias requeridas:

```bash
sudo apt install g++ libwebsocketpp-dev libboost-all-dev libssl-dev
```
2. Clona el repositorio:

```bash
git clone https://github.com/clementefeo/websockets-file-sender.git
cd websockets-file-sender
```
3. Compila el servidor:

```bash
g++ -std=c++11 -o websocket_server websocket_server.cpp -I ./websocketpp -lboost_system -lboost_thread -lssl -lcrypto
```
## Uso

Ejecuta el servidor con:

```bash
./websocket_server [opciones]
```

Opciones de línea de comandos

- -p <puerto>: Especifica el puerto a usar (por defecto: 8765)
- -i <IP>: Especifica la dirección IP en la que escuchar (por defecto: todas las IPs)
- -s <tamaño máximo en MB>: Especifica el tamaño máximo del mensaje en MB (por defecto: 200 MB)
- -l <ruta del archivo de registro>: Especifica la ruta para el archivo de registro (por defecto: syslog)
- -c <archivo de configuración>: Especifica un archivo de configuración personalizado
- -h: Muestra la información de ayuda

## Configuración como Servicio del Sistema

1. Crea un archivo de servicio:

```bash
sudo nano /etc/init.d/websocket_server
```
2. Copia el siguiente contenido en el archivo:

```bash
#!/bin/bash
# /etc/init.d/websocket_server

### BEGIN INIT INFO
# Provides:          websocket_server
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Servidor WebSocket
# Description:       Inicia el servidor WebSocket
### END INIT INFO

DIR=/usr/local/bin/websocket_server
DAEMON=$DIR/websocket_server
DAEMON_NAME=websocket_server

# Añade cualquier opción de línea de comandos para tu daemon aquí
DAEMON_OPTS=""

# Esta siguiente línea determina como que usuario se ejecuta el script.
DAEMON_USER=root

# El ID del proceso del script cuando se ejecuta se almacena aquí:
PIDFILE=/var/run/$DAEMON_NAME.pid

. /lib/lsb/init-functions

do_start() {
    log_daemon_msg "Iniciando el daemon del sistema $DAEMON_NAME"
    start-stop-daemon --start --background --pidfile $PIDFILE --make-pidfile --user $DAEMON_USER --exec $DAEMON -- $DAEMON_OPTS
    log_end_msg $?
}
do_stop() {
    log_daemon_msg "Deteniendo el daemon del sistema $DAEMON_NAME"
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
        echo "Uso: /etc/init.d/$DAEMON_NAME {start|stop|restart|status}"
        exit 1
        ;;
esac
exit 0
```

3. Haz que el archivo de servicio sea ejecutable:

```bash
sudo chmod +x /etc/init.d/websocket_server
```

4. Mueve el servidor compilado al directorio apropiado:

```bash
sudo mkdir -p /usr/local/bin/websocket_server
sudo mv websocket_server /usr/local/bin/websocket_server/
```

5. Crea un archivo de configuración

Crea un archivo de configuración en `/etc/websocket_server/websocket_server.conf` con el siguiente contenido:

```bash
sudo mkdir -p /etc/websocket_server
sudo nano /etc/websocket_server/websocket_server.conf
```

Añade el siguiente contenido de ejemplo al archivo:

```bash
# Configuración del Servidor WebSocket

# Puerto en el que escuchar
port=8765

# Dirección IP a la que enlazar (usar 0.0.0.0 para todas las interfaces)
ip=0.0.0.0

# Tamaño máximo del mensaje en MB
max_message_size=200

# Ruta del archivo de registro (dejar vacío para usar syslog)
log_file=/var/log/websocket_server.log
```

6. Registra el script en los niveles de ejecución predeterminados:

```bash
sudo update-rc.d websocket_server defaults
```

7. Inicia el servicio:

```bash
sudo service websocket_server start
```
8. Verifica el estado del servicio:

```bash
sudo service websocket_server status
```

## Contribuir
Por favor, lee CONTRIBUTING.md para detalles sobre nuestro código de conducta y el proceso para enviarnos pull requests.

## Licencia
Este proyecto está licenciado bajo la Licencia MIT - ver el archivo LICENSE.md para más detalles.
