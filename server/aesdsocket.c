#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>

volatile sig_atomic_t hastostop = 0;

void terminate(int signum) {
    hastostop = 1;
}

void rcvpacket(int sock, int file, struct sockaddr_in *sndr) {
    char c, *buffer;
    ssize_t n;

    while ((n = recv(sock, &c, 1, 0)) > 0) {
        write(file, &c, 1);
        if (c == '\n') break;
    }

    long file_size = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET);

    if (file_size > 0) {
        buffer = malloc(file_size);
        if (buffer != NULL) {
            if (read(file, buffer, file_size) == (ssize_t)file_size) {
                send(sock, buffer, file_size, 0);
            }
            free(buffer);
        }
    }

    lseek(file, 0, SEEK_END);
    syslog(LOG_DEBUG, "Closed connection with %s", inet_ntoa(sndr->sin_addr));
    close(sock);
}

int main(int argc, char* argv[]) {
    pid_t pid;
    int sock, singlesock, file;
    struct addrinfo hints, *res;
    struct sockaddr_in sndr;
    socklen_t sndrlen;

    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction newaction;
    memset(&newaction, 0, sizeof(struct sigaction));
    newaction.sa_handler = terminate;
    if (sigaction(SIGTERM, &newaction, NULL) != 0 || sigaction(SIGINT, &newaction, NULL) != 0) {
        return -1;
    }

    file = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (file == -1) return -1;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        close(file);
        return -1;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, "9000", &hints, &res) != 0) {
        close(sock); close(file);
        return -1;
    }

    if (bind(sock, res->ai_addr, res->ai_addrlen) != 0) {
        freeaddrinfo(res);
        close(sock); close(file);
        return -1;
    }
    freeaddrinfo(res);
    if(argc>1)
        pid=fork();

    if(argc==1 || pid == 0){
        if(argc > 1){
            setsid();
            chdir("/");
            int fd = open("/dev/null", O_RDWR);
            if (fd != -1) {
                dup2(fd, STDIN_FILENO);  
                dup2(fd, STDOUT_FILENO); 
                dup2(fd, STDERR_FILENO); 
                if (fd > 2) close(fd);
            }
        }
        if (listen(sock, 10) != 0) {
            close(sock); close(file);
            return -1;
        }

        while (!hastostop) {
            sndrlen = sizeof(struct sockaddr_in);
            singlesock = accept(sock, (struct sockaddr *)&sndr, &sndrlen);
            
            if (singlesock == -1) {
                if (errno == EINTR) {
                    break;
                }
                continue; 
            }

            syslog(LOG_DEBUG, "Accepted connection from %s", inet_ntoa(sndr.sin_addr));
            rcvpacket(singlesock, file, &sndr);
        }

        syslog(LOG_USER, "Caught signal, exiting");
        close(sock);
        close(file);
        unlink("/var/tmp/aesdsocketdata");
        closelog();
    }
    else if (argc>1 && pid !=0)
        exit(0);
    return 0;
}