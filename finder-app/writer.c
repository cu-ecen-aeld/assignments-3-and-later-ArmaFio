#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    openlog(NULL, 0, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Missing arguments");
        return 1;
    }

    char *filename = argv[1];
    char *text = argv[2];

    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        syslog(LOG_ERR, "Impossibile aprire il file: %s", filename);
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", text, filename);
    fprintf(fp, "%s", text);

    fclose(fp);
    closelog();

    return 0;
}
