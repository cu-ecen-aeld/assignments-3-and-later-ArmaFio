#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>     // Per fork(), execv(), dup2(), close()
#include <sys/types.h>  // Per il tipo pid_t

bool do_system(const char *command);

bool do_exec(int count, ...);

bool do_exec_redirect(const char *outputfile, int count, ...);
