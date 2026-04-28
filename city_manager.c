#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>


typedef struct {
    time_t timestamp;            
    int report_id;              
    int severity;        
    float lat;                   
    float lon;                   
    char inspector_name[50];    
    char category[30];          
    char description[100];     
} Report;                       

typedef enum { ROLE_UNKNOWN, ROLE_INSPECTOR, ROLE_MANAGER } Role;


void get_permissions_string(mode_t mode, char *out_str) {
    strcpy(out_str, "---------");
    
    if (mode & S_IRUSR) out_str[0] = 'r';
    if (mode & S_IWUSR) out_str[1] = 'w';
    if (mode & S_IXUSR) out_str[2] = 'x';
    
    if (mode & S_IRGRP) out_str[3] = 'r';
    if (mode & S_IWGRP) out_str[4] = 'w';
    if (mode & S_IXGRP) out_str[5] = 'x';
    
    if (mode & S_IROTH) out_str[6] = 'r';
    if (mode & S_IWOTH) out_str[7] = 'w';
    if (mode & S_IXOTH) out_str[8] = 'x';
}


int validate_access(const char *path, Role role, char mode_req) {
    struct stat st;
    if (stat(path, &st) < 0) {
        return 1; 
    }

    mode_t mode = st.st_mode;
    if (role == ROLE_MANAGER) {
        if (mode_req == 'r') return (mode & S_IRUSR);
        if (mode_req == 'w') return (mode & S_IWUSR);
    } else if (role == ROLE_INSPECTOR) {
        if (mode_req == 'r') return (mode & S_IRGRP);
        if (mode_req == 'w') return (mode & S_IWGRP);
    }
    return 0;
}


void log_district_activity(const char *dist, const char *user, const char *role_name, const char *action) {
    char log_path[512];
    snprintf(log_path, 512, "%s/logged_district", dist);

    int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("Failed to open log file");
        return;
    }

    char entry[1024];
    time_t now = time(NULL);
    int len = snprintf(entry, 1024, "%ld | User: %s | Role: %s | Action: %s\n", 
                       (long)now, user, role_name, action);
    
    write(fd, entry, len);
    chmod(log_path, 0644);
    close(fd);
}


void initialize_district_storage(const char *dist) {
    struct stat st;
    char path[512];

    if (stat(dist, &st) < 0) {
        if (mkdir(dist, 0750) < 0) {
            perror("Error creating district directory");
            return;
        }
    }
    chmod(dist, 0750);

    snprintf(path, 512, "%s/reports.dat", dist);
    int fd = open(path, O_CREAT | O_RDWR, 0664);
    if (fd >= 0) close(fd);
    chmod(path, 0664);

    snprintf(path, 512, "%s/district.cfg", dist);
    if (stat(path, &st) < 0) {
        fd = open(path, O_WRONLY | O_CREAT, 0640);
        if (fd >= 0) {
            write(fd, "threshold=1\n", 12);
            close(fd);
        }
    }
    chmod(path, 0640);

    snprintf(path, 512, "%s/logged_district", dist);
    fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd >= 0) close(fd);
    chmod(path, 0644);

    char link_name[512], r_path[512];
    snprintf(r_path, 512, "%s/reports.dat", dist);
    snprintf(link_name, 512, "active_reports-%s", dist);

    struct stat lst;
    if (lstat(link_name, &lst) == 0) {
        if (S_ISLNK(lst.st_mode) && stat(link_name, &st) < 0) {
            printf("Warning: Recreating dangling symlink for district %s\n", dist);
            unlink(link_name);
            symlink(r_path, link_name);
        }
    } else {
        symlink(r_path, link_name);
    }
}


void cmd_add_report(const char *dist, Role role, const char *user, const char *role_str) {
    char path[512];
    snprintf(path, 512, "%s/reports.dat", dist);

    if (!validate_access(path, role, 'w')) {
        fprintf(stderr, "Permission Denied: Role %s cannot write to %s\n", role_str, path);
        return;
    }

    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd < 0) {
        perror("Could not open reports file for writing");
        return;
    }

    Report r;
    memset(&r, 0, sizeof(Report));

    struct stat st;
    fstat(fd, &st);
    r.report_id = (int)(st.st_size / sizeof(Report)) + 1;
    r.timestamp = time(NULL);
    strncpy(r.inspector_name, user, 49);

    printf("Enter GPS Latitude: ");  scanf("%f", &r.lat);
    printf("Enter GPS Longitude: "); scanf("%f", &r.lon);
    printf("Category: "); scanf("%29s", r.category);
    printf("Severity (1-3): "); scanf("%d", &r.severity);
    
    while (getchar() != '\n'); 
    printf("Description: ");
    fgets(r.description, 100, stdin);
    r.description[strcspn(r.description, "\n")] = 0;

    // Phase 1 Binary Write
    write(fd, &r, sizeof(Report));
    close(fd);

    /* ════════════════════════════════════════════════════════════
       PHASE 2: NOTIFY MONITOR VIA SIGNAL
       ════════════════════════════════════════════════════════════ */
    int pid_fd = open(".monitor_pid", O_RDONLY);
    char notify_status[128];

    if (pid_fd < 0) {
        // Error case: Monitor isn't running or file is missing
        snprintf(notify_status, sizeof(notify_status), 
                 "Monitor could not be informed (PID file missing)");
    } else {
        char pid_buf[16];
        ssize_t bytes_read = read(pid_fd, pid_buf, sizeof(pid_buf) - 1);
        close(pid_fd);

        if (bytes_read > 0) {
            pid_buf[bytes_read] = '\0';
            pid_t monitor_pid = (pid_t)atoi(pid_buf);
            if (kill(monitor_pid, SIGUSR1) == 0) {
                snprintf(notify_status, sizeof(notify_status), 
                         "Monitor (PID %d) notified successfully", monitor_pid);
            } else {
                snprintf(notify_status, sizeof(notify_status), 
                         "Monitor could not be informed (Signal failed)");
            }
        } else {
            snprintf(notify_status, sizeof(notify_status), 
                     "Monitor could not be informed (Empty PID file)");
        }
    }
    log_district_activity(dist, user, role_str, notify_status);
    
    printf("Report #%d added. %s\n", r.report_id, notify_status);
}

void cmd_list_reports(const char *dist, Role role, const char *user, const char *role_str) {
    char path[512];
    snprintf(path, 512, "%s/reports.dat", dist);

    if (!validate_access(path, role, 'r')) {
        fprintf(stderr, "Permission Denied: Role %s cannot read %s\n", role_str, path);
        return;
    }

    struct stat st;
    if (stat(path, &st) < 0) {
        perror("Error accessing report file");
        return;
    }

    char perms[11];
    get_permissions_string(st.st_mode, perms);

    printf("\n--- District Dashboard: %s ---\n", dist);
    printf("Metadata: %s | Bytes: %ld | Updated: %s", perms, (long)st.st_size, ctime(&st.st_mtime));

    int fd = open(path, O_RDONLY);
    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        printf("[%d] %-10s | Severity: %d | User: %s\n", 
               r.report_id, r.category, r.severity, r.inspector_name);
    }
    close(fd);
    log_district_activity(dist, user, role_str, "LIST_REPORTS");
}

void cmd_view_report(const char *dist, Role role, const char *user, const char *role_str, int target_id) {
    char path[512];
    snprintf(path, 512, "%s/reports.dat", dist);

    if (!validate_access(path, role, 'r')) {
        fprintf(stderr, "Permission Denied.\n");
        return;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return; }
    Report r;
    int found = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.report_id == target_id) {
            printf("\n════════════ REPORT #%d ════════════\n", r.report_id);
            printf(" Inspector  : %s\n", r.inspector_name);
            printf(" Coordinates: %.4f, %.4f\n", r.lat, r.lon);
            printf(" Category   : %s\n", r.category);
            printf(" Severity   : %d\n", r.severity);
            printf(" Created On : %s", ctime(&r.timestamp));
            printf(" Description: %s\n", r.description);
            printf("═════════════════════════════════════\n");
            found = 1;
            break;
        }
    }
    if (!found) printf("Error: Report ID %d not found.\n", target_id);
    close(fd);
    log_district_activity(dist, user, role_str, "VIEW_REPORT");
}

void cmd_remove_report(const char *dist, Role role, const char *user, const char *role_str, int target_id) {
    if (role != ROLE_MANAGER) {
        fprintf(stderr, "Error: Manager privilege required for deletion.\n");
        return;
    }

    char path[512];
    snprintf(path, 512, "%s/reports.dat", dist);

    int fd = open(path, O_RDWR);
    struct stat st;
    fstat(fd, &st);
    long total_records = st.st_size / sizeof(Report);
    int found_idx = -1;

    Report r;
    for (int i = 0; i < total_records; i++) {
        read(fd, &r, sizeof(Report));
        if (r.report_id == target_id) {
            found_idx = i;
            break;
        }
    }

    if (found_idx != -1) {
        Report temp;
        for (int i = found_idx + 1; i < total_records; i++) {
            lseek(fd, i * sizeof(Report), SEEK_SET);
            read(fd, &temp, sizeof(Report));
            lseek(fd, (i - 1) * sizeof(Report), SEEK_SET);
            write(fd, &temp, sizeof(Report));
        }
        ftruncate(fd, (total_records - 1) * sizeof(Report));
        printf("Record removed. Binary file truncated.\n");
    } else {
        printf("Error: ID %d not found.\n", target_id);
    }

    close(fd);
    log_district_activity(dist, user, role_str, "REMOVE_REPORT");
}

void cmd_update_threshold(const char *dist, Role role, const char *user, const char *role_str, int value) {
    if (role != ROLE_MANAGER) {
        fprintf(stderr, "Access Denied.\n");
        return;
    }

    char path[512];
    snprintf(path, 512, "%s/district.cfg", dist);

    struct stat st;
    if (stat(path, &st) < 0) return;

    if ((st.st_mode & 0777) != 0640) {
        printf("Security Error: Permissions mismatch on cfg file.\n");
        return;
    }

    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd >= 0) {
        dprintf(fd, "threshold=%d\n", value);
        close(fd);
        printf("Configuration updated to %d.\n", value);
    }
    log_district_activity(dist, user, role_str, "UPDATE_THRESHOLD");
}


int parse_condition(const char *input, char *field, char *op, char *value) {
    return sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3;
}

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int v = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity == v;
        if (strcmp(op, ">=") == 0) return r->severity >= v;
        if (strcmp(op, "<=") == 0) return r->severity <= v;
    } else if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->category, value) == 0;
    }
    return 0;
}

void cmd_filter(const char *dist, Role role, const char *user, const char *role_str, int n, char **conds) {
    char path[512];
    snprintf(path, 512, "%s/reports.dat", dist);

    if (!validate_access(path, role, 'r')) return;

    int fd = open(path, O_RDONLY);
    Report r;
    int matches = 0;

    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int all_ok = 1;
        for (int i = 0; i < n; i++) {
            char f[64], o[16], v[64];
            if (parse_condition(conds[i], f, o, v)) {
                if (!match_condition(&r, f, o, v)) {
                    all_ok = 0;
                    break;
                }
            }
        }
        if (all_ok) {
            printf("FOUND -> ID: %d | %s\n", r.report_id, r.category);
            matches++;
        }
    }
    close(fd);
    printf("Filter search complete: %d hits.\n", matches);
    log_district_activity(dist, user, role_str, "FILTER_REPORTS");
}

void cmd_remove_district(const char *dist, const char *role) {
    if (strcmp(role, "manager") != 0) {
        fprintf(stderr, "Only managers can remove districts.\n");
        return;
    }

    char symlink_name[256];
    sprintf(symlink_name, "active_reports-%s", dist);
    unlink(symlink_name); 
    pid_t pid = fork();
    if (pid == 0) {
        printf("[System] Child process %d deleting directory: %s\n", getpid(), dist);
        char *args[] = {"rm", "-rf", (char *)dist, NULL};
        execvp("rm", args);
        exit(1);
    } else {
        wait(NULL);
        printf("[System] District %s successfully removed.\n", dist);
    }
}

void notify_monitor(const char *dist) {
    int fd = open(".monitor_pid", O_RDONLY);
    char log_msg[256];

    if (fd >= 0) {
        char pid_buf[10];
        read(fd, pid_buf, 10);
        pid_t monitor_pid = atoi(pid_buf);
        close(fd);

        if (kill(monitor_pid, SIGUSR1) == 0) {
            sprintf(log_msg, "Monitor (PID %d) notified via SIGUSR1.", monitor_pid);
        } else {
            sprintf(log_msg, "Error: Could not send signal to Monitor.");
        }
    } else {
        sprintf(log_msg, "Error: .monitor_pid not found. Monitor not informed.");
    }
    log_district_activity(dist, "SYSTEM", "city_manager", log_msg);
    printf("%s\n", log_msg);
}

int main(int argc, char *argv[]) {
    Role role = ROLE_UNKNOWN;
    char *user = "anonymous", *cmd = NULL, *dist = NULL, *role_str = "guest";
    int extra_arg = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            role_str = argv[++i];
            if (strcmp(role_str, "manager") == 0) role = ROLE_MANAGER;
            else if (strcmp(role_str, "inspector") == 0) role = ROLE_INSPECTOR;
        } else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            user = argv[++i];
        } else if (cmd == NULL && argv[i][0] != '-') {
            cmd = argv[i];
        } else if (dist == NULL && argv[i][0] != '-') {
            dist = argv[i];
        } else if (argv[i][0] != '-') {
            extra_arg = atoi(argv[i]);
        }
    }

    if (!cmd || !dist || role == ROLE_UNKNOWN) {
        fprintf(stderr, "Usage: ./city_manager --role <role> --user <user> <cmd> <district> [args]\n");
        return 1;
    }

    if (sizeof(Report) != 208) {
        fprintf(stderr, "Fatal Error: Compiler padding error. Struct size is %zu.\n", sizeof(Report));
        return 1;
    }

    initialize_district_storage(dist);

    if (strcmp(cmd, "add") == 0) cmd_add_report(dist, role, user, role_str);
    else if (strcmp(cmd, "list") == 0) cmd_list_reports(dist, role, user, role_str);
    else if (strcmp(cmd, "view") == 0) cmd_view_report(dist, role, user, role_str, extra_arg);
    else if (strcmp(cmd, "remove_report") == 0) cmd_remove_report(dist, role, user, role_str, extra_arg);
    else if (strcmp(cmd, "update_threshold") == 0) cmd_update_threshold(dist, role, user, role_str, extra_arg);
    else if (strcmp(cmd, "filter") == 0) {
        cmd_filter(dist, role, user, role_str, argc - 7, &argv[7]);
    }

    return 0;
}