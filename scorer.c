#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#define USER_LEN     50
#define CATEGORY_LEN 30
#define DESC_LEN    100

typedef struct {
    int    id;
    char   inspector[USER_LEN];
    float  lat;
    float  lon;
    char   category[CATEGORY_LEN];
    int    severity;
    time_t timestamp;
    char   desc[DESC_LEN];
} Report;

#define MAX_INSPECTORS 64

typedef struct {
    char name[USER_LEN];
    int  score;
} InspectorScore;

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <district_id>\n", argv[0]);
        return 1;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", argv[1]);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "scorer: cannot open %s\n", path);
        return 1;
    }

    InspectorScore scores[MAX_INSPECTORS];
    int n_inspectors = 0;

    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int found = 0;
        for (int i = 0; i < n_inspectors; i++) {
            if (strcmp(scores[i].name, r.inspector) == 0) {
                scores[i].score += r.severity;
                found = 1;
                break;
            }
        }
        if (!found && n_inspectors < MAX_INSPECTORS) {
            strncpy(scores[n_inspectors].name, r.inspector, USER_LEN - 1);
            scores[n_inspectors].name[USER_LEN - 1] = '\0';
            scores[n_inspectors].score = r.severity;
            n_inspectors++;
        }
    }
    close(fd);

    for (int i = 0; i < n_inspectors; i++) {
        printf("%s %d\n", scores[i].name, scores[i].score);
    }

    return 0;
}