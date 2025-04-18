#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>
#include <sys/wait.h>

#include <openssl/bio.h>
#include <openssl/evp.h>

#define QUARANTINE_DIR "/home/juanz/Documents/Sisop/soal_2/quarantine"
#define STARTER_KIT    "/home/juanz/Documents/Sisop/soal_2/starter_kit"
#define PID_FILE       "/home/juanz/Documents/Sisop/soal_2/starterkit.pid"
#define LOG_FILE       "/home/juanz/Documents/Sisop/soal_2/activity.log"

/* ===================== BASE64 DECODE BEFORE ===================== */

// static char decoding_table[256];
// static int mod_table[] = {0, 2, 1};

// void build_decoding_table() {
//     const char encoding_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
//     for (int i = 0; i < 64; i++) {
//         decoding_table[(unsigned char)encoding_table[i]] = i;
//     }
// }

// char* base64_decode(const char *data, size_t input_length, size_t *output_length) {
//     if (decoding_table['A'] == 0) {
//         build_decoding_table();
//     }

//     if (input_length % 4 != 0) return NULL;

//     *output_length = input_length / 4 * 3;
//     if (data[input_length - 1] == '=') (*output_length)--;
//     if (data[input_length - 2] == '=') (*output_length)--;

//     char *decoded_data = malloc(*output_length + 1);
//     if (decoded_data == NULL) return NULL;

//     for (int i = 0, j = 0; i < input_length;) {
//         uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
//         uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
//         uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
//         uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];

//         uint32_t triple = (sextet_a << 18) +
//                           (sextet_b << 12) +
//                           (sextet_c << 6) +
//                           sextet_d;

//         if (j < *output_length) decoded_data[j++] = (triple >> 16) & 0xFF;
//         if (j < *output_length) decoded_data[j++] = (triple >> 8) & 0xFF;
//         if (j < *output_length) decoded_data[j++] = triple & 0xFF;
//     }

//     decoded_data[*output_length] = '\0';
//     return decoded_data;
// }

/* ===================== BASE64 DECODE AFTER ===================== */
char* base64_decode(const char* input) {
    BIO *bio, *b64;
    int len = strlen(input);
    char *buffer = malloc(len + 1);
    if (!buffer) return NULL;
    memset(buffer, 0, len + 1);

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf((void*)input, len);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    int decoded_size = BIO_read(bio, buffer, len);
    if (decoded_size < 0) decoded_size = 0;
    buffer[decoded_size] = '\0';
    BIO_free_all(bio);
    return buffer;
}

/* ===================== LOGGING ===================== */
void get_timestamp(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "[%d-%m-%Y][%H:%M:%S]", t);
}

void write_log(const char *log_entry) {
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) {
        fprintf(stderr, "Error opening log file %s: %s\n", LOG_FILE, strerror(errno));
        return;
    }
    fprintf(log_fp, "%s\n", log_entry);
    fclose(log_fp);
}

/* ===================== DIRECTORY CHECK ===================== */
void ensure_directory_exists(const char *dir) {
    struct stat st;
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0755) == -1) {
            fprintf(stderr, "Failed to create directory %s: %s\n", dir, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

/* ===================== DECRYPT FILES IN QUARANTINE ===================== */
void decrypt_files() {
    DIR *dir = opendir(QUARANTINE_DIR);
    if (!dir) {
        perror("Failed to open quarantine directory");
        ensure_directory_exists(QUARANTINE_DIR);
        return;
    }
    struct dirent *entry;
    char oldpath[512], newpath[512];
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(oldpath, sizeof(oldpath), "%s/%s", QUARANTINE_DIR, entry->d_name);
        char *decoded = base64_decode(entry->d_name);
        if (!decoded) continue;
        snprintf(newpath, sizeof(newpath), "%s/%s", QUARANTINE_DIR, decoded);
        if (rename(oldpath, newpath) == 0) {
            printf("Renamed: %s -> %s\n", entry->d_name, decoded);
        } else {
            perror("Failed to rename file");
        }
        free(decoded);
    }
    closedir(dir);
}

/* ===================== MOVE AND LOG ===================== */
void move_files(const char *src, const char *dst, const char *op) {
    ensure_directory_exists(src);
    ensure_directory_exists(dst);
    DIR *dir = opendir(src);
    if (!dir) {
        perror("Failed to open source directory");
        return;
    }
    struct dirent *entry;
    char oldpath[512], newpath[512];
    char timestamp[50], log_msg[300];
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(oldpath, sizeof(oldpath), "%s/%s", src, entry->d_name);
        snprintf(newpath, sizeof(newpath), "%s/%s", dst, entry->d_name);
        if (rename(oldpath, newpath) == 0) {
            printf("%s: %s -> %s\n", op, entry->d_name, dst);
            get_timestamp(timestamp, sizeof(timestamp));
            snprintf(log_msg, sizeof(log_msg), "%s - %s - Successfully %s to %s directory.",
                     timestamp, entry->d_name, op, dst);
            write_log(log_msg);
        } else {
            perror("Failed to move file");
        }
    }
    closedir(dir);
}

/* ===================== ERADICATE ===================== */
void eradicate() {
    DIR *dir = opendir(QUARANTINE_DIR);
    if (!dir) return;
    struct dirent *entry;
    char filepath[512], timestamp[50], log_msg[300];
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(filepath, sizeof(filepath), "%s/%s", QUARANTINE_DIR, entry->d_name);
        if (remove(filepath) == 0) {
            printf("Removed: %s\n", filepath);
            get_timestamp(timestamp, sizeof(timestamp));
            snprintf(log_msg, sizeof(log_msg), "%s - %s - Successfully deleted.", timestamp, entry->d_name);
            write_log(log_msg);
        } else {
            perror("Failed to delete file");
        }
    }
    closedir(dir);
}

/* ===================== RUN DAEMON ===================== */
void run_daemon() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    if (setsid() < 0) exit(EXIT_FAILURE);
    if (chdir("/") < 0) exit(EXIT_FAILURE);
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    FILE *fp = fopen(PID_FILE, "w");
    if (fp) {
        fprintf(fp, "%d", getpid());
        fclose(fp);
    }

    char timestamp[50], log_msg[300];
    get_timestamp(timestamp, sizeof(timestamp));
    snprintf(log_msg, sizeof(log_msg), "Decrypt daemon started %s - PID %d.", timestamp, getpid());
    write_log(log_msg);

    while (1) {
        decrypt_files();
        sleep(1);
    }
}

/* ===================== SHUTDOWN DAEMON ===================== */
void shutdown_daemon() {
    FILE *fp = fopen(PID_FILE, "r");
    if (!fp) return;
    int pid;
    fscanf(fp, "%d", &pid);
    fclose(fp);
    if (kill(pid, SIGTERM) == 0) {
        printf("Daemon with PID %d stopped.\n", pid);
        char timestamp[50], log_msg[300];
        get_timestamp(timestamp, sizeof(timestamp));
        snprintf(log_msg, sizeof(log_msg), "Shutdown:%s - PID %d stopped.", timestamp, pid);
        write_log(log_msg);
        remove(PID_FILE);
    } else {
        perror("Failed to stop daemon");
    }
}

/* ===================== DOWNLOAD & UNZIP ===================== */
void download_and_extract() {
    ensure_directory_exists(STARTER_KIT);
    char zip_path[512];
    snprintf(zip_path, sizeof(zip_path), "%s/starter_kit.zip", STARTER_KIT);
    const char *url = "https://drive.usercontent.google.com/u/0/uc?id=1_5GxIGfQr3mNKuavJbte_AoRkEQLXSKS&export=download";
    char *wget_args[] = {"wget", "-O", zip_path, (char*)url, NULL};
    pid_t pid = fork();
    if (pid == 0) {
        execvp("wget", wget_args);
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) return;
    }
    char *unzip_args[] = {"unzip", "-o", zip_path, "-d", STARTER_KIT, NULL};
    pid = fork();
    if (pid == 0) {
        execvp("unzip", unzip_args);
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) return;
    }
    remove(zip_path);
}

/* ===================== HELP ===================== */
void print_help(const char *prog) {
    printf("Usage: %s [--decrypt|--quarantine|--return|--eradicate|--shutdown]\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_help(argv[0]);
        return EXIT_FAILURE;
    }
    ensure_directory_exists(QUARANTINE_DIR);
    ensure_directory_exists(STARTER_KIT);

    if (strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
    } else if (strcmp(argv[1], "--decrypt") == 0) {
        download_and_extract();
        run_daemon();
    } else if (strcmp(argv[1], "--quarantine") == 0) {
        move_files(STARTER_KIT, QUARANTINE_DIR, "Quarantine");
    } else if (strcmp(argv[1], "--return") == 0) {
        move_files(QUARANTINE_DIR, STARTER_KIT, "Return");
    } else if (strcmp(argv[1], "--eradicate") == 0) {
        eradicate();
    } else if (strcmp(argv[1], "--shutdown") == 0) {
        shutdown_daemon();
    } else {
        print_help(argv[0]);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
Compile with: gcc starterkit_ref.c -o starterkit_ref -lcrypto
*/
