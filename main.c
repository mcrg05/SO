#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define PERM_DISTRICT_DIR   0750   
#define PERM_REPORTS_DAT    0664   
#define PERM_DISTRICT_CFG   0640
#define PERM_LOGGED         0644 

#define USER_LEN      50
#define CATEGORY_LEN  30
#define DESC_LEN      100

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

static char role[20];
static char user_name[50];
static char operation[30];
static char district_id[50];
static int  report_id = -1;
static int  threshold_val = -1;

#define MAX_CONDITIONS 8

static char filter_conditions[MAX_CONDITIONS][128];
static int  conditions = 0;

static void mode_to_str(mode_t mode, char out[10]) {
    out[0] = (mode & S_IRUSR) ? 'r' : '-';
    out[1] = (mode & S_IWUSR) ? 'w' : '-';
    out[2] = (mode & S_IXUSR) ? 'x' : '-';
    out[3] = (mode & S_IRGRP) ? 'r' : '-';
    out[4] = (mode & S_IWGRP) ? 'w' : '-';
    out[5] = (mode & S_IXGRP) ? 'x' : '-';
    out[6] = (mode & S_IROTH) ? 'r' : '-';
    out[7] = (mode & S_IWOTH) ? 'w' : '-';
    out[8] = (mode & S_IXOTH) ? 'x' : '-';
    out[9] = '\0';
}

static void path_reports(char *buf, size_t n, const char *district) {
    snprintf(buf, n, "%s/reports.dat", district);
}
static void path_cfg(char *buf, size_t n, const char *district) {
    snprintf(buf, n, "%s/district.cfg", district);
}
static void path_log(char *buf, size_t n, const char *district) {
    snprintf(buf, n, "%s/logged_district", district);
}
static void path_symlink(char *buf, size_t n, const char *district) {
    snprintf(buf, n, "active_reports-%s", district);
}

static void log_action(const char *district, const char *action) {   
    char log_path[256];
    path_log(log_path, sizeof(log_path), district);

    struct stat st;
    if (stat(log_path, &st) == 0) {
        if (strcmp(role, "inspector") == 0) {
            return;
        }
    }

    int fd = open(log_path, O_WRONLY | O_APPEND | O_CREAT, PERM_LOGGED);
    if (fd < 0) return;
    chmod(log_path, PERM_LOGGED);

    time_t now = time(NULL);
    char line[256];
    snprintf(line, sizeof(line), "%ld\t%s\t%s\t%s\n",
             (long)now, user_name, role, action);
    write(fd, line, strlen(line));
    close(fd);
}

static int ensure_district(const char *district) {
    struct stat st;

    if (stat(district, &st) != 0) {
        if (mkdir(district, PERM_DISTRICT_DIR) != 0) {
            perror("mkdir");
            return -1;
        }
    }
    chmod(district, PERM_DISTRICT_DIR);

    char cfg[256];
    path_cfg(cfg, sizeof(cfg), district);
    if (stat(cfg, &st) != 0) {
        int fd = open(cfg, O_WRONLY | O_CREAT | O_TRUNC, PERM_DISTRICT_CFG);
        if (fd < 0) { perror("open district.cfg"); return -1; }
        const char *default_cfg = "severity_threshold=1\n";
        write(fd, default_cfg, strlen(default_cfg));
        close(fd);
    }
    chmod(cfg, PERM_DISTRICT_CFG);

    char log_path[256];
    path_log(log_path, sizeof(log_path), district);
    if (stat(log_path, &st) != 0) {
        int fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, PERM_LOGGED);
        if (fd < 0) { perror("open logged_district"); return -1; }
        close(fd);
    }
    chmod(log_path, PERM_LOGGED);

    char rep[256];
    path_reports(rep, sizeof(rep), district);
    if (stat(rep, &st) == 0) {
        chmod(rep, PERM_REPORTS_DAT);
    }

    char symlink_name[256];
    path_symlink(symlink_name, sizeof(symlink_name), district);
    char target[256];
    path_reports(target, sizeof(target), district);

    struct stat lst;
    if (lstat(symlink_name, &lst) == 0) {
        unlink(symlink_name);
    }
    symlink(target, symlink_name);

    return 0;
}

static int check_permission(const char *filepath, char access_needed) {
    struct stat st;
    if (stat(filepath, &st) != 0) {
        return 0;
    }
    mode_t m = st.st_mode;

    int allowed = 0;
    if (strcmp(role, "manager") == 0) {
        allowed = (access_needed == 'r') ? (m & S_IRUSR) : (m & S_IWUSR);
    } else {
        allowed = (access_needed == 'r') ? (m & S_IRGRP) : (m & S_IWGRP);
    }

    if (!allowed) {
        char perm_str[10];
        mode_to_str(m, perm_str);
        fprintf(stderr,
                "ERROR: role '%s' does not have %s access to '%s' (perms: %s)\n",
                role,
                access_needed == 'r' ? "read" : "write",
                filepath,
                perm_str);
        return -1;
    }
    return 0;
}

static int next_report_id(const char *rep_path) {
    int fd = open(rep_path, O_RDONLY);
    if (fd < 0) return 1;
    Report r;
    int max_id = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id > max_id) max_id = r.id;
    }
    close(fd);
    return max_id + 1;
}

static void op_add(void) {
    if (ensure_district(district_id) != 0) return;

    char rep_path[256];
    path_reports(rep_path, sizeof(rep_path), district_id);

    if (check_permission(rep_path, 'w') != 0) return;

    Report r;
    memset(&r, 0, sizeof(Report));

    r.id = next_report_id(rep_path);
    strncpy(r.inspector, user_name, USER_LEN - 1);
    r.timestamp = time(NULL);

    printf("X: ");   if (scanf("%f", &r.lat)  != 1) { fprintf(stderr, "Bad input\n"); return; }
    printf("Y: ");   if (scanf("%f", &r.lon)  != 1) { fprintf(stderr, "Bad input\n"); return; }
    printf("Category: ");
    if (scanf("%29s", r.category) != 1) { fprintf(stderr, "Bad input\n"); return; }
    printf("Severity (1/2/3): ");
    if (scanf("%d", &r.severity)  != 1) { fprintf(stderr, "Bad input\n"); return; }
    {
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }
    printf("Description: ");
    if (fgets(r.desc, DESC_LEN, stdin) == NULL) { fprintf(stderr, "Bad input\n"); return; }
    r.desc[strcspn(r.desc, "\n")] = '\0';

    int fd = open(rep_path, O_WRONLY | O_APPEND | O_CREAT, PERM_REPORTS_DAT);
    if (fd < 0) { perror("open reports.dat"); return; }
    chmod(rep_path, PERM_REPORTS_DAT);

    if (write(fd, &r, sizeof(Report)) != sizeof(Report)) {
        perror("write");
    } else {
        printf("Report #%d added to district '%s'.\n", r.id, district_id);
    }
    close(fd);

    char symlink_name[256];
    path_symlink(symlink_name, sizeof(symlink_name), district_id);
    char target[256];
    path_reports(target, sizeof(target), district_id);
    if (unlink(symlink_name) == 0 || errno == ENOENT) {
        symlink(target, symlink_name);
    }

    log_action(district_id, "add");
}

static void op_list(void) {
    char rep_path[256];
    path_reports(rep_path, sizeof(rep_path), district_id);

    if (check_permission(rep_path, 'r') != 0) return;

    struct stat st;
    if (stat(rep_path, &st) != 0) {
        printf("No reports file for district '%s'.\n", district_id);
        return;
    }

    char perm_str[10];
    mode_to_str(st.st_mode, perm_str);
    char time_buf[64];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("=== District: %s ===\n", district_id);
    printf("File: %s | Permissions: %s | Size: %lld bytes | Last modified: %s\n\n",
           rep_path, perm_str, (long long)st.st_size, time_buf);

    char symlink_name[256];
    path_symlink(symlink_name, sizeof(symlink_name), district_id);
    struct stat lst;
    if (lstat(symlink_name, &lst) == 0 && S_ISLNK(lst.st_mode)) {
        struct stat target_st;
        if (stat(symlink_name, &target_st) != 0) {
            fprintf(stderr, "WARNING: symlink '%s' is dangling (target missing).\n",
                    symlink_name);
        }
    }

    int fd = open(rep_path, O_RDONLY);
    if (fd < 0) { perror("open reports.dat"); return; }

    Report r;
    int count = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        char ts_buf[32];
        struct tm *t = localtime(&r.timestamp);
        strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%d %H:%M:%S", t);
        printf("[%d] Inspector: %-15s | Pos: (%.4f, %.4f) | Category: %-12s | Severity: %d | %s\n",
               r.id, r.inspector, r.lat, r.lon, r.category, r.severity, ts_buf);
        printf("    Description: %s\n", r.desc);
        count++;
    }
    close(fd);
    if (count == 0) printf("(no reports)\n");

    log_action(district_id, "list");
}

static void op_view(void) {
    char rep_path[256];
    path_reports(rep_path, sizeof(rep_path), district_id);

    if (check_permission(rep_path, 'r') != 0) return;

    int fd = open(rep_path, O_RDONLY);
    if (fd < 0) { perror("open reports.dat"); return; }

    Report r;
    int found = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == report_id) {
            char ts_buf[32];
            struct tm *t = localtime(&r.timestamp);
            strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%d %H:%M:%S", t);
            printf("=== Report #%d ===\n", r.id);
            printf("Inspector : %s\n",   r.inspector);
            printf("Position  : (%.6f, %.6f)\n", r.lat, r.lon);
            printf("Category  : %s\n",   r.category);
            printf("Severity  : %d\n",   r.severity);
            printf("Timestamp : %s\n",   ts_buf);
            printf("Description: %s\n",  r.desc);
            found = 1;
            break;
        }
    }
    close(fd);

    if (!found) {
        fprintf(stderr, "Report #%d not found in district '%s'.\n",
                report_id, district_id);
    }

    log_action(district_id, "view");
}

static void op_remove_report(void) {
    if (strcmp(role, "manager") != 0) {
        fprintf(stderr, "ERROR: remove_report requires manager role.\n");
        return;
    }

    char rep_path[256];
    path_reports(rep_path, sizeof(rep_path), district_id);

    if (check_permission(rep_path, 'w') != 0) return;

    int fd = open(rep_path, O_RDWR);
    if (fd < 0) { perror("open reports.dat"); return; }

    struct stat st;
    fstat(fd, &st);
    off_t file_size = st.st_size;
    int   total     = (int)(file_size / sizeof(Report));

    int target_idx = -1;
    Report r;
    for (int i = 0; i < total; i++) {
        lseek(fd, (off_t)(i * sizeof(Report)), SEEK_SET);
        if (read(fd, &r, sizeof(Report)) != sizeof(Report)) break;
        if (r.id == report_id) { target_idx = i; break; }
    }

    if (target_idx < 0) {
        fprintf(stderr, "Report #%d not found.\n", report_id);
        close(fd);
        return;
    }

    for (int i = target_idx; i < total - 1; i++) {
        lseek(fd, (off_t)((i + 1) * sizeof(Report)), SEEK_SET);
        read(fd, &r, sizeof(Report));
        lseek(fd, (off_t)(i * sizeof(Report)), SEEK_SET);
        write(fd, &r, sizeof(Report));
    }

    ftruncate(fd, (off_t)((total - 1) * sizeof(Report)));
    close(fd);

    printf("Report #%d removed from district '%s'.\n", report_id, district_id);
    log_action(district_id, "remove_report");
}

static void op_update_threshold(void) {
    if (strcmp(role, "manager") != 0) {
        fprintf(stderr, "ERROR: update_threshold requires manager role.\n");
        return;
    }

    char cfg_path[256];
    path_cfg(cfg_path, sizeof(cfg_path), district_id);

    struct stat st;
    if (stat(cfg_path, &st) != 0) {
        fprintf(stderr, "district.cfg not found for district '%s'.\n", district_id);
        return;
    }
    mode_t actual = st.st_mode & 0777;
    if (actual != PERM_DISTRICT_CFG) {
        char perm_str[10];
        mode_to_str(st.st_mode, perm_str);
        fprintf(stderr,
                "ERROR: district.cfg permissions are %s (expected rw-r-----). "
                "Refusing to write.\n", perm_str);
        return;
    }

    if (check_permission(cfg_path, 'w') != 0) return;

    int fd = open(cfg_path, O_WRONLY | O_TRUNC);
    if (fd < 0) { perror("open district.cfg"); return; }

    char buf[64];
    snprintf(buf, sizeof(buf), "severity_threshold=%d\n", threshold_val);
    write(fd, buf, strlen(buf));
    close(fd);

    printf("Severity threshold for '%s' updated to %d.\n",
           district_id, threshold_val);
    log_action(district_id, "update_threshold");
}

int parse_condition(const char *input, char *field, char *op, char *value) {
    char tmp[128];
    strncpy(tmp, input, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *p = strchr(tmp, ':');
    if (!p) return -1;
    *p = '\0';
    strcpy(field, tmp);

    char *rest = p + 1;
    p = strchr(rest, ':');
    if (!p) return -1;
    *p = '\0';
    strcpy(op, rest);

    strcpy(value, p + 1);
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  AI-ASSISTED: match_condition
 *  Returns 1 if report r satisfies field op value, else 0.
 * ════════════════════════════════════════════════════════════════════════════ */
int match_condition(Report *r, const char *field, const char *op,
                    const char *value) {
    if (strcmp(field, "severity") == 0) {
        int threshold = atoi(value);
        int sev = r->severity;
        if      (strcmp(op, "==") == 0) return sev == threshold;
        else if (strcmp(op, "!=") == 0) return sev != threshold;
        else if (strcmp(op, "<")  == 0) return sev <  threshold;
        else if (strcmp(op, "<=") == 0) return sev <= threshold;
        else if (strcmp(op, ">")  == 0) return sev >  threshold;
        else if (strcmp(op, ">=") == 0) return sev >= threshold;
    } else if (strcmp(field, "category") == 0) {
        int cmp = strcmp(r->category, value);
        if      (strcmp(op, "==") == 0) return cmp == 0;
        else if (strcmp(op, "!=") == 0) return cmp != 0;
    } else if (strcmp(field, "inspector") == 0) {
        int cmp = strcmp(r->inspector, value);
        if      (strcmp(op, "==") == 0) return cmp == 0;
        else if (strcmp(op, "!=") == 0) return cmp != 0;
    } else if (strcmp(field, "timestamp") == 0) {
        time_t threshold = (time_t)atol(value);
        time_t ts = r->timestamp;
        if      (strcmp(op, "==") == 0) return ts == threshold;
        else if (strcmp(op, "!=") == 0) return ts != threshold;
        else if (strcmp(op, "<")  == 0) return ts <  threshold;
        else if (strcmp(op, "<=") == 0) return ts <= threshold;
        else if (strcmp(op, ">")  == 0) return ts >  threshold;
        else if (strcmp(op, ">=") == 0) return ts >= threshold;
    }
    return 0;
}

static void op_filter(void) {
    char rep_path[256];
    path_reports(rep_path, sizeof(rep_path), district_id);

    if (check_permission(rep_path, 'r') != 0) return;

    int fd = open(rep_path, O_RDONLY);
    if (fd < 0) { perror("open reports.dat"); return; }

    printf("=== Filter results for district '%s' ===\n", district_id);
    int count = 0;
    Report r;

    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int all_match = 1;
        for (int i = 0; i < conditions; i++) {
            char field[32], op_str[4], value[64];
            if (parse_condition(filter_conditions[i], field, op_str, value) != 0) {
                fprintf(stderr, "Bad condition: %s\n", filter_conditions[i]);
                all_match = 0;
                break;
            }
            if (!match_condition(&r, field, op_str, value)) {
                all_match = 0;
                break;
            }
        }
        if (all_match) {
            char ts_buf[32];
            struct tm *t = localtime(&r.timestamp);
            strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%d %H:%M:%S", t);
            printf("[%d] %s | (%.4f,%.4f) | %s | sev:%d | %s\n    %s\n",
                   r.id, r.inspector, r.lat, r.lon,
                   r.category, r.severity, ts_buf, r.desc);
            count++;
        }
    }
    close(fd);

    if (count == 0) printf("(no matching reports)\n");
    else printf("--- %d match(es) ---\n", count);

    log_action(district_id, "filter");
}

static void parse_arguments(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s --role <manager|inspector> --user <name> "
            "--<operation> [district] [args...]\n", argv[0]);
        exit(1);
    }

    if (strcmp(argv[1], "--role") != 0) { fprintf(stderr, "Expected --role\n"); exit(1); }
    strncpy(role, argv[2], sizeof(role) - 1);

    if (strcmp(argv[3], "--user") != 0) { fprintf(stderr, "Expected --user\n"); exit(1); }
    strncpy(user_name, argv[4], sizeof(user_name) - 1);

    if (argc < 6) { fprintf(stderr, "Expected an operation flag.\n"); exit(1); }

    const char *op_flag = argv[5];
    int i = 6;

    if (strcmp(op_flag, "--add") == 0) {
        strcpy(operation, "add");
        if (i >= argc) { fprintf(stderr, "--add requires <district>\n"); exit(1); }
        strncpy(district_id, argv[i++], sizeof(district_id) - 1);

    } else if (strcmp(op_flag, "--list") == 0) {
        strcpy(operation, "list");
        if (i >= argc) { fprintf(stderr, "--list requires <district>\n"); exit(1); }
        strncpy(district_id, argv[i++], sizeof(district_id) - 1);

    } else if (strcmp(op_flag, "--view") == 0) {
        strcpy(operation, "view");
        if (i + 1 >= argc) { fprintf(stderr, "--view requires <district> <report_id>\n"); exit(1); }
        strncpy(district_id, argv[i++], sizeof(district_id) - 1);
        report_id = atoi(argv[i++]);

    } else if (strcmp(op_flag, "--remove_report") == 0) {
        strcpy(operation, "remove_report");
        if (i + 1 >= argc) { fprintf(stderr, "--remove_report requires <district> <report_id>\n"); exit(1); }
        strncpy(district_id, argv[i++], sizeof(district_id) - 1);
        report_id = atoi(argv[i++]);

    } else if (strcmp(op_flag, "--update_threshold") == 0) {
        strcpy(operation, "update_threshold");
        if (i + 1 >= argc) { fprintf(stderr, "--update_threshold requires <district> <value>\n"); exit(1); }
        strncpy(district_id, argv[i++], sizeof(district_id) - 1);
        threshold_val = atoi(argv[i++]);

    } else if (strcmp(op_flag, "--filter") == 0) {
        strcpy(operation, "filter");
        if (i >= argc) { fprintf(stderr, "--filter requires <district> <condition...>\n"); exit(1); }
        strncpy(district_id, argv[i++], sizeof(district_id) - 1);
        while (i < argc && conditions < MAX_CONDITIONS) {
            strncpy(filter_conditions[conditions++], argv[i++],
                    sizeof(filter_conditions[0]) - 1);
        }
    } else {
        fprintf(stderr, "Unknown operation: %s\n", op_flag);
        exit(1);
    }
}

int main(int argc, char **argv) {
    parse_arguments(argc, argv);

    /* Validate role */
    if (strcmp(role, "manager") != 0 && strcmp(role, "inspector") != 0) {
        fprintf(stderr, "ERROR: role must be 'manager' or 'inspector'.\n");
        return 1;
    }

    if (strcmp(operation, "add") == 0) {
        op_add();
    } else if (strcmp(operation, "list") == 0) {
        op_list();
    } else if (strcmp(operation, "view") == 0) {
        op_view();
    } else if (strcmp(operation, "remove_report") == 0) {
        op_remove_report();
    } else if (strcmp(operation, "update_threshold") == 0) {
        op_update_threshold();
    } else if (strcmp(operation, "filter") == 0) {
        op_filter();
    }

    return 0;
}