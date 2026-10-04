#! /bin/sh

case "$1" in 
    start)
        echo "Starting aesdsocket"
        /usr/bin/aesdchar_load
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket"
        start-stop-daemon -K -n aesdsocket
        /usr/bin/aesdchar_unload
        ;;
    *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac
exit 0