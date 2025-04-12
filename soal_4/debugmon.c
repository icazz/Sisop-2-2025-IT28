#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define LOG_FILE "debugmon.log"
#define PID_FILE_FORMAT "debugmon_%s.pid"
#define FAIL_LOCK_FORMAT "fail_%s.lock"
#define BUFFER_SIZE 4096

// UTILITAS 

void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "[%d:%m:%Y]-[%H:%M:%S]", tm_info);
}

void log_status(const char *proc_name, const char *status) {
    FILE *log = fopen(LOG_FILE, "a");
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    if (log) {
        fprintf(log, "%s_%s_%s\n", timestamp, proc_name, status);
        fclose(log);
    }
}

uid_t get_uid_by_name(const char *username) {
    struct passwd *pw = getpwnam(username);
    if (!pw) {
        fprintf(stderr, "User '%s' not found.\n", username);
        exit(EXIT_FAILURE);
    }
    return pw->pw_uid;
}

// LIST 

void list_processes(const char *username) {
    uid_t target_uid = get_uid_by_name(username);
    DIR *proc = opendir("/proc");
    struct dirent *entry;

    if (!proc) {
        perror("opendir /proc");
        return;
    }

    printf("%-8s %-6s %-6s %-6s %s\n", "USER", "PID", "CPU%", "MEM%", "COMMAND");

    while ((entry = readdir(proc))) {
        if (!isdigit(entry->d_name[0])) continue;

        char status_path[BUFFER_SIZE], cmd_path[BUFFER_SIZE], buffer[BUFFER_SIZE];
        snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
        snprintf(cmd_path, sizeof(cmd_path), "/proc/%s/cmdline", entry->d_name);

        FILE *fp = fopen(status_path, "r");
        if (!fp) continue;

        uid_t uid = -1;
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strncmp(buffer, "Uid:", 4) == 0) {
                sscanf(buffer, "Uid:\t%d", &uid);
                break;
            }
        }
        fclose(fp);
        if (uid != target_uid) continue;

        char command[BUFFER_SIZE] = "-";
        fp = fopen(cmd_path, "r");
        if (fp) {
            size_t len = fread(command, 1, sizeof(command) - 1, fp);
            fclose(fp);
            if (len > 0) {
                command[len] = '\0';
                for (size_t i = 0; i < len; i++) {
                    if (command[i] == '\0') command[i] = ' ';
                }
            }
        }

        printf("%-8s %-6s %-6s %-6s %s\n", username, entry->d_name, "-", "-", command);
    }

    closedir(proc);
}

// DAEMON 

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);
}

void run_daemon(const char *username) {
    char pidfile[64];
    snprintf(pidfile, sizeof(pidfile), PID_FILE_FORMAT, username);

    daemonize();

    FILE *fp = fopen(pidfile, "w");
    if (fp) {
        fprintf(fp, "%d\n", getpid());
        fclose(fp);
    }

    while (1) {
        DIR *proc = opendir("/proc");
        struct dirent *entry;
        uid_t uid = get_uid_by_name(username);

        if (proc) {
            while ((entry = readdir(proc))) {
                if (!isdigit(entry->d_name[0])) continue;

                char status_path[BUFFER_SIZE];
                snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
                FILE *fp = fopen(status_path, "r");
                if (!fp) continue;

                uid_t proc_uid = -1;
                char buffer[BUFFER_SIZE];
                while (fgets(buffer, sizeof(buffer), fp)) {
                    if (strncmp(buffer, "Uid:", 4) == 0) {
                        sscanf(buffer, "Uid:\t%d", &proc_uid);
                        break;
                    }
                }
                fclose(fp);

                if (proc_uid == uid) {
                    log_status(entry->d_name, "RUNNING");
                }
            }
            closedir(proc);
        }

        sleep(5);
    }
}

// STOP 

void stop_daemon(const char *username) {
    char pidfile[64];
    snprintf(pidfile, sizeof(pidfile), PID_FILE_FORMAT, username);

    FILE *fp = fopen(pidfile, "r");
    if (!fp) {
        fprintf(stderr, "Daemon for %s not running.\n", username);
        return;
    }

    int pid;
    fscanf(fp, "%d", &pid);
    fclose(fp);

    if (kill(pid, SIGTERM) == 0) {
        printf("Daemon stopped.\n");
        log_status("debugmon", "RUNNING");
        remove(pidfile);
    } else {
        perror("Failed to stop daemon");
    }
}

// FAIL 
void fail_user(const char *username) {
    uid_t uid = get_uid_by_name(username);
    DIR *proc = opendir("/proc");
    struct dirent *entry;

    if (!proc) {
        perror("opendir /proc");
        return;
    }

    while ((entry = readdir(proc))) {
        if (!isdigit(entry->d_name[0])) continue;

        char status_path[BUFFER_SIZE];
        snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
        FILE *fp = fopen(status_path, "r");
        if (!fp) continue;

        uid_t proc_uid = -1;
        char buffer[BUFFER_SIZE];
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strncmp(buffer, "Uid:", 4) == 0) {
                sscanf(buffer, "Uid:\t%d", &proc_uid);
                break;
            }
        }
        fclose(fp);

        if (proc_uid != uid) continue;

        int pid = atoi(entry->d_name);
        if (pid == getpid()) continue;

        kill(pid, SIGKILL);
        log_status(entry->d_name, "FAILED");
    }

    closedir(proc);

    char lockfile[64];
    snprintf(lockfile, sizeof(lockfile), FAIL_LOCK_FORMAT, username);
    FILE *lock = fopen(lockfile, "w");
    if (lock) fclose(lock);
    printf("FAIL mode aktif untuk user %s.\n", username);
}

//  REVERT

void revert_user(const char *username) {
    char lockfile[64];
    snprintf(lockfile, sizeof(lockfile), FAIL_LOCK_FORMAT, username);
    if (remove(lockfile) == 0) {
        log_status("debugmon", "RUNNING");
        printf("Revert sukses. User %s dapat menjalankan proses lagi.\n", username);
    } else {
        printf("Tidak ada FAIL mode aktif untuk user %s.\n", username);
    }
}

// MAIN 

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s list <user>\n", argv[0]);
        fprintf(stderr, "  %s daemon <user>\n", argv[0]);
        fprintf(stderr, "  %s stop <user>\n", argv[0]);
        fprintf(stderr, "  %s fail <user>\n", argv[0]);
        fprintf(stderr, "  %s revert <user>\n", argv[0]);
        return 1;
    }

    const char *command = argv[1];
    const char *username = argv[2];

    if (strcmp(command, "list") == 0) {
        list_processes(username);
    } else if (strcmp(command, "daemon") == 0) {
        run_daemon(username);
    } else if (strcmp(command, "stop") == 0) {
        stop_daemon(username);
    } else if (strcmp(command, "fail") == 0) {
        fail_user(username);
    } else if (strcmp(command, "revert") == 0) {
        revert_user(username);
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
    }

    return 0;
}

