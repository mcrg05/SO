#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<time.h>

#define DISTRICT_DIRECTORY_PERMISSIONS 750
#define REPORTSDAT_PERMISSIONS 664
#define DISTRICTCFG_PERMISSIONS 640
#define LOGGED_DISTRICT_PERMISSIONS 664

char role[10], user[20], operation[20], d_id[20];
int r_id = 0;

typedef struct {
    int id;
    char user[50];
    float lat;
    float lon;
    char category[30];
    int severity;
    time_t timestamp;
    char desc[100];
} REPORT_T;

void parse_arguments(int argc, char **argv){
    if (argv[1][0] != '-') exit(1);
    if (argv[1][0] != '-') exit(1);

    strcpy(role,argv[2]);
    strcpy(user, argv[4]);

    //printf("%s\n", role);
    int i = 5;
    while (i < argc){
        if (argv[i][0] != '-') exit(1);
        if (argv[i][1] != '-') exit(1);
        if (strcmp(argv[i], "--add") == 0) {
            strcpy(operation, "add");
            strcpy(d_id, argv[i + 1]);
            i += 2;
        }
        else if (strcmp(argv[i], "--list") == 0) {
            strcpy(operation, "list");
            strcpy(d_id, argv[i + 1]);
            i += 2;
        }
        else if (strcmp(argv[i], "--view") == 0) {
            strcpy(operation, "view");
            strcpy(d_id, argv[i + 1]);
            i += 2;
        }
        else if (strcmp(argv[i], "--remove_report") == 0) {
            strcpy(operation, "remove_report");
            strcpy(d_id, argv[i + 1]);
            r_id = atoi(argv[i + 2]);
            i += 3;
        }
        else if (strcmp(argv[i], "--update_threshold") == 0) {
            strcpy(operation, "update_threshold");
            i += 3;
        }
    }
}

int main(int argc, char **argv) {
    //city_manager --role inspector --add downtown
    parse_arguments(argc, argv);

    printf("role: %s; user: %s; operation: %s; d_id: %s; r_id: %d", role, user, operation, d_id, r_id);

    if (strcmp(operation, "--add") == 0) {

    }
    else if (strcmp(operation, "--list") == 0) {
    
    }
    else if (strcmp(operation, "--view") == 0) {

    }
    else if (strcmp(operation, "--remove_report") == 0) {

    }
    else if (strcmp(operation, "--update_threshold") == 0) {

    }

    return 0;
}