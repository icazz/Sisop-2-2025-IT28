#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <regex.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

void run_command(char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        perror("exec gagal");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        perror("fork gagal");
    }
}

void download_and_extract() {
    struct stat st = {0};
    if (stat("Clues", &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("Folder Clues sudah ada. Melewati download.\n");
        return;
    }

    printf("Mendownload dan mengekstrak Clues.zip...\n");

    // 1. wget -q -O Clues.zip URL
    char *wget_args[] = {
        "wget", "-q", "-O", "Clues.zip",
        "https://drive.usercontent.google.com/u/0/uc?id=1xFn1OBJUuSdnApDseEczKhtNzyGekauK&export=download",
        NULL
    };
    run_command(wget_args);

    // 2. unzip -q Clues.zip
    char *unzip_args[] = {"unzip", "-q", "Clues.zip", NULL};
    run_command(unzip_args);

    // 3. rm Clues.zip
    unlink("Clues.zip");

    // 4. Jika ada folder Clues/Clues, pindahkan isinya ke Clues/
    DIR *inner = opendir("Clues/Clues");
    if (inner != NULL) {
        struct dirent *entry;
        while ((entry = readdir(inner)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            char old_path[PATH_MAX], new_path[PATH_MAX];
            snprintf(old_path, sizeof(old_path), "Clues/Clues/%s", entry->d_name);
            snprintf(new_path, sizeof(new_path), "Clues/%s", entry->d_name);
            rename(old_path, new_path);
        }
        closedir(inner);
        rmdir("Clues/Clues");
    }
}

void filter_recursive(const char *base_path, regex_t *regex) {
    DIR *dir = opendir(base_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);

        struct stat st;
        if (stat(path, &st) == -1) continue;

        if (S_ISDIR(st.st_mode)) {
            // Rekursi ke subfolder
            filter_recursive(path, regex);
        } else if (S_ISREG(st.st_mode)) {
            // Cek nama file apakah cocok dengan pola
            if (regexec(regex, entry->d_name, 0, NULL, 0) == 0) {
                // Pindahkan file ke Filtered
                char dst[PATH_MAX];
                snprintf(dst, sizeof(dst), "Filtered/%s", entry->d_name);
                if (rename(path, dst) != 0) {
                    perror("Gagal memindahkan file");
                } else {
                    printf("Dipindahkan: %s\n", path);
                }
            } else {
                // Hapus file yang tidak cocok
                if (remove(path) != 0) {
                    perror("Gagal menghapus file");
                }
            }
        }
    }

    closedir(dir);
}

void filter_files() {
    struct stat st = {0};
    if (stat("Filtered", &st) == -1) {
        if (mkdir("Filtered", 0755) != 0) {
            perror("Gagal membuat folder Filtered");
            return;
        }
    }

    regex_t regex;
    int ret = regcomp(&regex, "^[a-zA-Z0-9]\\.txt$", REG_EXTENDED);
    if (ret) {
        char errbuf[128];
        regerror(ret, &regex, errbuf, sizeof(errbuf));
        fprintf(stderr, "Gagal compile regex: %s\n", errbuf);
        return;
    }

    // Mulai filter dari folder Clues
    filter_recursive("Clues", &regex);

    regfree(&regex);
    printf("Filter selesai.\n");
}


void combine_files() {
    FILE *out = fopen("Combined.txt", "w");
    if (!out) {
        perror("Gagal membuat Combined.txt");
        return;
    }

    char path[PATH_MAX];
    char buffer[1024];

    for (int i = 1; i <= 9; ++i) {
        // Gabungkan angka 1-9
        snprintf(path, sizeof(path), "Filtered/%d.txt", i);
        FILE *in = fopen(path, "r");
        if (in) {
            size_t n;
            while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
                fwrite(buffer, 1, n, out);
            }
            fclose(in);
            remove(path);
        }

        // Gabungkan huruf a-i
        char ch = 'a' + (i - 1);  // 'a' sampai 'i'
        snprintf(path, sizeof(path), "Filtered/%c.txt", ch);
        in = fopen(path, "r");
        if (in) {
            size_t n;
            while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
                fwrite(buffer, 1, n, out);
            }
            fclose(in);
            remove(path);
        }
    }

    fclose(out);
    printf("Combined.txt berhasil dibuat!\n");
}


char rot13(char c) {
    if ('a' <= c && c <= 'z') return 'a' + (c - 'a' + 13) % 26;
    if ('A' <= c && c <= 'Z') return 'A' + (c - 'A' + 13) % 26;
    return c;
}

void decode_file() {
    FILE *in = fopen("Combined.txt", "r");
    FILE *out = fopen("Decoded.txt", "w");
    if (!in || !out) {
        perror("Gagal membuka file");
        return;
    }

    int ch;
    while ((ch = fgetc(in)) != EOF) {
        fputc(rot13(ch), out);
    }

    fclose(in);
    fclose(out);
    printf("Decode selesai. File disimpan di Decoded.txt\n");
}

void print_usage() {
    printf("-------------------------------------------------------------------------\n");
    printf("|                                HELP                                   |\n");
    printf("-------------------------------------------------------------------------\n");
    printf("|./action                | Download dan extract Clues.zip jika belum ada|\n");
    printf("|./action -m Filter      | Memfilter file ke dalam folder Filtered      |\n");
    printf("|./action -m Combine     | Gabungkan isi file Filtered ke Combined.txt  |\n");
    printf("|./action -m Decode      | Dekripsi Combined.txt jadi Decoded.txt       |\n");
    printf("-------------------------------------------------------------------------\n");
}

int main(int argc, char* argv[]) {

    if (argc == 1) {
        download_and_extract();
    } else if (argc == 3 && strcmp(argv[1], "-m") == 0) {
        if (strcmp(argv[2], "Filter") == 0) filter_files();
        else if (strcmp(argv[2], "Combine") == 0) combine_files();
        else if (strcmp(argv[2], "Decode") == 0) decode_file();
        else print_usage();
    } else {
        print_usage();
    }

    return 0;
}