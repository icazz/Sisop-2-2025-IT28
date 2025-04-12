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

#define QUARANTINE_DIR "/home/juanz/Documents/Sisop/soal_2/quarantine"
#define STARTER_KIT    "/home/juanz/Documents/Sisop/soal_2/starter_kit"
#define PID_FILE       "/home/juanz/Documents/Sisop/soal_2/starterkit.pid"
#define LOG_FILE       "/home/juanz/Documents/Sisop/soal_2/activity.log" 

/* ===================== BASE64 DECODE ===================== */

static char decoding_table[256];
static int mod_table[] = {0, 2, 1};

void build_decoding_table() {
    const char encoding_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; i++) {
        decoding_table[(unsigned char)encoding_table[i]] = i;
    }
}

char* base64_decode(const char *data, size_t input_length, size_t *output_length) {
    if (decoding_table['A'] == 0) {
        build_decoding_table();
    }

    if (input_length % 4 != 0) return NULL;

    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;

    char *decoded_data = malloc(*output_length + 1);
    if (decoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < input_length;) {
        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];

        uint32_t triple = (sextet_a << 18) +
                          (sextet_b << 12) +
                          (sextet_c << 6) +
                          sextet_d;

        if (j < *output_length) decoded_data[j++] = (triple >> 16) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = triple & 0xFF;
    }

    decoded_data[*output_length] = '\0';
    return decoded_data;
}

/* ===================== LOGGING ===================== */

void get_timestamp(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "[%d-%m-%Y][%H:%M:%S]", t);
}

void write_log(const char *log_entry) {
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (log_fp == NULL) {
        fprintf(stderr, "Error opening log file %s: %s\n", LOG_FILE, strerror(errno));
        return;
    }
    fprintf(log_fp, "%s\n", log_entry);
    fclose(log_fp);
}

/* ===================== PENGECEKAN DIREKTORI ===================== */

void ensure_directory_exists(const char *dir) {
    struct stat st;
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0755) == -1) {
            fprintf(stderr, "Gagal membuat direktori %s: %s\n", dir, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

/* ===================== DEKRIPSI FILE ===================== */

void decrypt_files() {
    DIR *dir = opendir(QUARANTINE_DIR);
    if (!dir) {
        perror("Gagal membuka direktori karantina");
        printf("\nProses untuk membuat direktori karantina");
        ensure_directory_exists(QUARANTINE_DIR);
        printf("\nProses membuat direktori karantina selesai");
        return;
    }
    struct dirent *entry;
    char oldname[256], newname[256];
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        strcpy(oldname, QUARANTINE_DIR);
        strcat(oldname, "/");
        strcat(oldname, entry->d_name);

        size_t output_length;
        char *decoded = base64_decode(entry->d_name, strlen(entry->d_name), &output_length);
        if (decoded) {
            strcpy(newname, QUARANTINE_DIR);
            strcat(newname, "/");
            strcat(newname, decoded);
            if (rename(oldname, newname) == 0) {
                printf("Renamed: %s -> %s\n", entry->d_name, decoded);
            } else {
                perror("Gagal mengganti nama file");
            }
            free(decoded);
        }
    }
    closedir(dir);
}

/* ===================== FUNGSI DAEMON ===================== */

void run_daemon() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork gagal");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);
    if (setsid() < 0) exit(EXIT_FAILURE);
    if (chdir("/") < 0) exit(EXIT_FAILURE);
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    FILE *fp = fopen(PID_FILE, "w");
    if (fp) {
        char pid_str[30];
        sprintf(pid_str, "%d", getpid());
        fputs(pid_str, fp);
        fclose(fp);
    }
    
    char timestamp[50];
    get_timestamp(timestamp, sizeof(timestamp));
    char log_msg[300];
    strcpy(log_msg, "Decrypt:\n");
    strcat(log_msg, timestamp);
    strcat(log_msg, " - Successfully started decryption process with PID ");
    {
        char temp[30];
        sprintf(temp, "%d", getpid());
        strcat(log_msg, temp);
    }
    strcat(log_msg, ".");
    write_log(log_msg);

    while (1) {
        decrypt_files();
        sleep(1);
    }
}

/* ===================== MOVE, HAPUS, SHUTDOWN ===================== */

void move_files(const char *src, const char *dst, const char *op) {
    ensure_directory_exists(src);
    ensure_directory_exists(dst);
    
    DIR *dir = opendir(src);
    if (!dir) {
        perror("Gagal membuka direktori sumber");
        return;
    }
    
    char header[100];
    strcpy(header, op);
    strcat(header, ":");
    write_log(header);
    
    struct dirent *entry;
    char oldpath[256], newpath[256];
    char log_msg[300];
    char timestamp[50];
    
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        strcpy(oldpath, src);
        strcat(oldpath, "/");
        strcat(oldpath, entry->d_name);

        strcpy(newpath, dst);
        strcat(newpath, "/");
        strcat(newpath, entry->d_name);

        if (rename(oldpath, newpath) == 0) {
            printf("Moved: %s -> %s\n", entry->d_name, dst);
            get_timestamp(timestamp, sizeof(timestamp));
            if (strcmp(op, "Quarantine") == 0) {
                strcpy(log_msg, timestamp);
                strcat(log_msg, " - ");
                strcat(log_msg, entry->d_name);
                strcat(log_msg, " - Successfully moved to quarantine directory.");
            } else if (strcmp(op, "Return") == 0) {
                strcpy(log_msg, timestamp);
                strcat(log_msg, " - ");
                strcat(log_msg, entry->d_name);
                strcat(log_msg, " - Successfully returned to starter kit directory.");
            }
            write_log(log_msg);
        } else {
            perror("Gagal memindahkan file");
        }
    }
    closedir(dir);
}

void eradicate() {
    DIR *dir = opendir(QUARANTINE_DIR);
    if (!dir) {
        perror("Gagal membuka direktori karantina");
        return;
    }
    
    write_log("Eradicate:");
    
    struct dirent *entry;
    char filepath[256];
    char timestamp[50];
    char log_msg[300];
    
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        strcpy(filepath, QUARANTINE_DIR);
        strcat(filepath, "/");
        strcat(filepath, entry->d_name);

        if (remove(filepath) == 0) {
            printf("Removed: %s\n", filepath);
            get_timestamp(timestamp, sizeof(timestamp));
            strcpy(log_msg, timestamp);
            strcat(log_msg, " - ");
            strcat(log_msg, entry->d_name);
            strcat(log_msg, " - Successfully deleted.");
            write_log(log_msg);
        } else {
            perror("Gagal menghapus file");
        }
    }
    closedir(dir);
}

void shutdown_daemon() {
    FILE *fp = fopen(PID_FILE, "r");
    if (!fp) {
        fprintf(stderr, "PID file tidak ditemukan. Apakah daemon sedang berjalan?\n");
        return;
    }
    int pid;
    fscanf(fp, "%d", &pid);
    fclose(fp);
    if (kill(pid, SIGTERM) == 0) {
        printf("Daemon dengan PID %d dimatikan.\n", pid);
        char timestamp[50];
        get_timestamp(timestamp, sizeof(timestamp));
        char log_msg[300];
        strcpy(log_msg, "Shutdown:\n");
        strcat(log_msg, timestamp);
        strcat(log_msg, " - Successfully shut off decryption process with PID ");
        {
            char temp[30];
            sprintf(temp, "%d", pid);
            strcat(log_msg, temp);
        }
        strcat(log_msg, ".");
        write_log(log_msg);
        remove(PID_FILE);
    } else {
        perror("Gagal mematikan daemon");
    }
}

/* ===================== PROSES DOWNLOAD & UNZIP ===================== */

void download_and_extract() {
    ensure_directory_exists(STARTER_KIT);

    char zip_path[300];
    strcpy(zip_path, STARTER_KIT);
    strcat(zip_path, "/starter_kit.zip");
    const char *download_url = "https://drive.usercontent.google.com/u/0/uc?id=1_5GxIGfQr3mNKuavJbte_AoRkEQLXSKS&export=download";
    char *wget_args[] = { "wget", "-O", zip_path, (char *)download_url, NULL };
    pid_t pid = fork();
    if(pid == 0) {
        execvp("wget", wget_args);
        perror("execvp failed untuk wget");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Error saat mendownload starter_kit.zip\n");
            return;
        }
    } else {
        perror("Fork gagal untuk wget");
        return;
    }

    char *unzip_args[] = { "unzip", "-o", zip_path, "-d", STARTER_KIT, NULL };
    pid = fork();
    if(pid == 0) {
        execvp("unzip", unzip_args);
        perror("execvp failed untuk unzip");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Error saat mengekstrak starter_kit.zip\n");
            return;
        }
    } else {
        perror("Fork gagal untuk unzip");
        return;
    }

    if (remove(zip_path) != 0) {
        perror("Gagal menghapus file starter_kit.zip");
    }
}

/* ===================== HELP ===================== */

void print_help(const char *progname) {
    printf("Usage: %s [OPTION]\n", progname);
    printf("  --decrypt    Menjalankan daemon untuk mendekripsi nama file di direktori quarantine\n");
    printf("  --quarantine Memindahkan file dari direktori starter_kit ke direktori quarantine\n");
    printf("  --return     Memindahkan file dari direktori quarantine ke direktori starter_kit\n");
    printf("  --eradicate  Menghapus semua file di direktori quarantine\n");
    printf("  --shutdown   Mematikan daemon yang sedang berjalan\n");
    printf("  --help       Menampilkan pesan bantuan ini\n");
}

/* ===================== MAIN ===================== */

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
