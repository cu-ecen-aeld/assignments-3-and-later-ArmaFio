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
#include <sys/queue.h>
#include <pthread.h>

typedef struct threadlist {
    pthread_t th;
    int completed;
    SLIST_ENTRY (threadlist) entries;
} threadlist_t;

typedef struct tdata{
    int file;
    pthread_mutex_t *mutex;
}tdata_t;

SLIST_HEAD (head, threadlist);

typedef struct rcvpcktparams{
    int sock;
    int file;
    int *oncompleted;
    struct sockaddr_in sndr;
    pthread_mutex_t *mutex;
} rcvpcktparams_t;

volatile sig_atomic_t hastostop = 0;

void terminate(int signum) {
    hastostop = 1;
}


void * rcvpacket(void *thread_params) {
    rcvpcktparams_t *parameters = (rcvpcktparams_t *) thread_params;
    char c, *buffer, *new_ptr;
    ssize_t n;
    int i=0, capacity = 512;

    buffer = malloc (sizeof(char)* capacity);
    while (((n = recv(parameters -> sock, &c, 1, 0)) > 0)||(n == -1 && errno == EINTR))  {
        if(i==(capacity)){
            capacity = capacity *2;
            new_ptr = realloc (buffer, capacity);
            if (new_ptr == NULL){
                free (buffer);
                syslog(LOG_DEBUG, "Closed connection with %s", inet_ntoa(parameters->sndr.sin_addr));
                close(parameters->sock);
                *(parameters->oncompleted) = 1;
                free(parameters);
                pthread_exit((void*)-1);
            }
            buffer = new_ptr;
        }
        buffer[i] = c;
        i++;
        if (c == '\n')
            break;
    }
    if (n <= 0){
        syslog(LOG_DEBUG, "Closed connection with %s", inet_ntoa(parameters->sndr.sin_addr));
        close(parameters->sock);
        *(parameters->oncompleted) = 1;
        free(parameters);
        pthread_exit((void*)-1);
    }
    pthread_mutex_lock(parameters->mutex);
    write (parameters->file, buffer, i);
    free(buffer);

    long file_size = lseek(parameters -> file, 0, SEEK_END);
    lseek(parameters -> file, 0, SEEK_SET);

    if (file_size > 0) {
        buffer = malloc(file_size);
        if (buffer != NULL) {
            if (read(parameters->file, buffer, file_size) == (ssize_t)file_size) {
                if((n = send(parameters->sock, buffer, file_size, 0)) <=0){
                    pthread_mutex_unlock(parameters->mutex);
                    syslog(LOG_DEBUG, "Closed connection with %s", inet_ntoa(parameters->sndr.sin_addr));
                    close(parameters->sock);
                    *(parameters->oncompleted) = 1;
                    pthread_exit((void *)-1);
                }
            }
            free(buffer);
        }
    }
    lseek(parameters->file, 0, SEEK_END);
    pthread_mutex_unlock(parameters->mutex);
    syslog(LOG_DEBUG, "Closed connection with %s", inet_ntoa(parameters->sndr.sin_addr));
    close(parameters->sock);
    *(parameters->oncompleted) = 1;
    free(parameters);
    pthread_exit(0);
}

 static void writetime(sigval_t sigval){
    tdata_t *td = (tdata_t *) sigval.sival_ptr;
    time_t rawtime;
    struct tm *info;
    char time_str[64];
    char final_str[100];
    int length;

    time(&rawtime);
    info = localtime(&rawtime);

    strftime(time_str, sizeof(time_str), "%a, %d %b %Y %H:%M:%S %z", info);
    length = snprintf(final_str, 100, "timestamp:%s\n", time_str); 
    pthread_mutex_lock (td -> mutex);
    write(td->file, final_str, length);
    pthread_mutex_unlock(td->mutex);
}

int main(int argc, char* argv[]) {
    int clockid = CLOCK_MONOTONIC;
    pthread_mutex_t *mutex;
    rcvpcktparams_t *params;
    struct head l;
    threadlist_t *data= NULL;
    pid_t pid;
    int sock, singlesock, file;
    struct addrinfo hints, *res;
    struct sockaddr_in sndr;
    socklen_t sndrlen;
    sigevent_t sev;
    tdata_t *td;
    timer_t timerid;
    struct itimerspec its;

    its.it_interval.tv_sec = 10;
    its.it_interval.tv_nsec=0;
    its.it_value.tv_sec = 10;
    its.it_value.tv_nsec = 0;


    td = malloc (sizeof(tdata_t));
    sev.sigev_notify = SIGEV_THREAD;
    sev._sigev_un._sigev_thread._function= writetime;
    sev.sigev_value.sival_ptr = td;


    SLIST_INIT(&l);
    mutex = malloc (sizeof(pthread_mutex_t));
    pthread_mutex_init(mutex, NULL);
    td ->mutex = mutex;
    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction newaction;
    memset(&newaction, 0, sizeof(struct sigaction));
    newaction.sa_handler = terminate;
    if (sigaction(SIGTERM, &newaction, NULL) != 0 || sigaction(SIGINT, &newaction, NULL) != 0) {
        return -1;
    }

    file = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (file == -1) return -1;
    td->file = file;

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
        timer_create(clockid, &sev, &timerid);
        timer_settime(timerid, 0, &its, NULL);
        if (listen(sock, SOMAXCONN) != 0) {
            close(sock); close(file);
            return -1;
        }
        while (!hastostop) {
            data = SLIST_FIRST(&l);
            while (data != NULL) {
                threadlist_t *next_data = SLIST_NEXT(data, entries); 
                if (data->completed == 1) {
                    pthread_join(data->th, NULL);
                    SLIST_REMOVE(&l, data, threadlist, entries);
                    free(data);
                }
                data = next_data; 
            }
            sndrlen = sizeof(struct sockaddr_in);
            singlesock = accept(sock, (struct sockaddr *)&sndr, &sndrlen);
            
            if (singlesock == -1) {
                if (errno == EINTR) {
                    break;
                }
                continue; 
            }
            
            syslog(LOG_DEBUG, "Accepted connection from %s", inet_ntoa(sndr.sin_addr));
            data = malloc(sizeof(threadlist_t));
            params = malloc(sizeof(rcvpcktparams_t));
            params->file = file;
            params->mutex = mutex;
            params->oncompleted = &(data->completed);
            params->sndr = sndr;
            params ->sock = singlesock;
            data->completed = 0;
            SLIST_INSERT_HEAD(&l, data, entries);
            pthread_create(&(data->th), NULL, rcvpacket, params);
        }
      
        while (!SLIST_EMPTY(&l)) {
            data = SLIST_FIRST(&l);         
            pthread_join(data->th, NULL);     
            SLIST_REMOVE_HEAD(&l, entries);   
            free(data);                       
        }
        timer_delete(timerid);
        free(td);       
        pthread_mutex_destroy(mutex);
        free(mutex);
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