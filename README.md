# Laporan Praktikum IT28 Modul 2
## Anggota Kelompok
| No |             Nama              |     NRP     |
|----|-------------------------------|-------------|
| 1  | Yuan Banny Albyan             | 5027241027  |
| 2  | Ica Zika Hamizah              | 5027241058  |
| 3  | Nafis Faqih Allmuzaky Maolidi | 5027241095  |

## Soal_1
### A. Downloading the Clues
Untuk bagian A pertama mengunduh file ZIP yang bernama Clues.zip pada link `https://drive.usercontent.google.com/u/0/uc?id=1xFn1OBJUuSdnApDseEczKhtNzyGekauK&export=download` lalu unzip file tersebut secara langsung, setelah di unzip Clues.zip dihapus. Isi dari Clues tersebut berupa 4 folder yakni ClueA - ClueD dan di masing-masing folder tersebut terdapat .txt files dan isinya masih tidak jelas.

```
void download_and_extract() {
    struct stat st = {0};
    if (stat("Clues", &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("Folder Clues sudah ada. Melewati download.\n");
        return;
    }
    ...
}
```
`stat("Clues", &st)` digunakan untuk memeriksa apakah folder Clues sudah ada di direktori kerja. Jika sudah ada, program akan menampilkan pesan "Folder Clues sudah ada" dan melewati proses unduh serta ekstraksi

```
char *wget_args[] = {
        "wget", "-q", "-O", "Clues.zip",
        "https://drive.usercontent.google.com/u/0/uc?id=1xFn1OBJUuSdnApDseEczKhtNzyGekauK&export=download",
        NULL
    };
    run_command(wget_args);
```
Menggunakan perintah `wget` untuk mengunduh file ZIP dari URL yang diberikan dan menyimpannya dengan nama `Clues.zip`.
Fungsi `run_command(wget_args)` akan menjalankan perintah ini di proses anak (fork), yang memanfaatkan `execvp` untuk mengeksekusi perintah `wget`.

```
char *unzip_args[] = {"unzip", "-q", "Clues.zip", NULL};
    run_command(unzip_args);
```
Menggunakan perintah unzip untuk mengekstrak isi dari `Clues.zip` tanpa menampilkan output (karena menggunakan opsi -q).

```
unlink("Clues.zip");
```
Setelah file diekstrak, file Clues.zip dihapus menggunakan fungsi unlink() untuk membersihkan file ZIP yang sudah tidak dibutuhkan lagi.

```
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
```
Jika folder Clues/Clues ditemukan setelah diekstrak, program akan membuka folder tersebut, membaca isinya, dan memindahkan semua file ke folder Clues/.
Setiap file di dalam folder Clues/Clues akan dipindahkan menggunakan fungsi `rename()`, dan setelah selesai, folder Clues/Clues akan dihapus menggunakan `rmdir()`.

FUNGSI PENDUKUNG
```
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
```
Fork: Program membuat proses anak dengan `fork()`. Jika `fork()` berhasil, proses anak akan memiliki `PID 0`, sedangkan proses induk (parent) akan menerima PID proses anak.

Exec: Pada proses anak, perintah sistem seperti `wget` atau `unzip` dijalankan dengan `execvp()`. Jika eksekusi gagal, program akan mencetak pesan error dan keluar.

Wait: Proses induk akan menunggu sampai proses anak selesai dengan `waitpid()`. Setelah proses anak selesai, program induk akan melanjutkan eksekusi.

bash:
```
./action
```

![Unzip](assets/unzip.png)

### B. Filtering the Files
Selanjutnya pindahkan file-file yang hanya dinamakan dengan 1 huruf dan 1 angka tanpa special character kedalam folder bernama Filtered. Karena terdapat banyak clue yang tidak berguna, jadi disaat melakukan filtering, file yang tidak terfilter dihapus.

```
void filter_files() {
    struct stat st = {0};
    if (stat("Filtered", &st) == -1) {
        if (mkdir("Filtered", 0755) != 0) {
            perror("Gagal membuat folder Filtered");
            return;
        }
    }
}
```
Fungsi `filter_files()` mengecek apakah folder Filtered/ sudah ada. Jika belum, maka folder tersebut dibuat dengan permission 0755.

```
regex_t regex;
int ret = regcomp(&regex, "^[a-zA-Z0-9]\\.txt$", REG_EXTENDED);
```
- ^[a-zA-Z0-9]\\.txt$

    - ^ → awal string

    - [a-zA-Z0-9] → satu karakter huruf/angka

    - \\.txt → harus diikuti .txt

    - $ → akhir string

- Contoh sesuai: `a.txt`, `d.txt`, `5.txt`
- Contoh tidak sesuai: `rj.txt`, `p0.txt`, `hint.txt`, `$.txt`

```
filter_recursive("Clues", &regex);
regfree(&regex);
```
Program menelusuri isi folder Clues/ secara rekursif untuk memproses file.

```
void filter_recursive(const char *base_path, regex_t *regex) {
    DIR *dir = opendir(base_path);
    ...
}
```
Membuka direktori base_path, dan membaca setiap entri file/subfolder.

```
while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
}
```
- Mengabaikan `.` dan `..`

- Membentuk path absolut ke `file/subfolder`

```
struct stat st;
if (stat(path, &st) == -1) continue;
```
Cek apakah file bisa diakses

```
if (S_ISDIR(st.st_mode)) {
    filter_recursive(path, regex);
}
```
Masuk ke subfolder jika ditemukan

```
else if (S_ISREG(st.st_mode)) {
    if (regexec(regex, entry->d_name, 0, NULL, 0) == 0) {
        ...
    } else {
        ...
    }
}
```
Gunakan `regexec()` untuk mencocokkan nama file terhadap `regex`.

```
char dst[PATH_MAX];
snprintf(dst, sizeof(dst), "Filtered/%s", entry->d_name);
if (rename(path, dst) != 0) {
    perror("Gagal memindahkan file");
} else {
    printf("Dipindahkan: %s\n", path);
}
```
Jika cocok → Pindahkan ke folder Filtered/

```
if (remove(path) != 0) {
    perror("Gagal menghapus file");
}
```
Jika tidak cocok maka file dihapus

bash:
```
./action --Filter
```

![Filtered](assets/filtered.png)

### C. Combine the File Content
Pada bagian ini, setelah file berhasil di filter pada directory `Filtered`, ambil isi dari setiap `.txt` file tersebut kedalam satu file yaitu `Combined.txt` dengan menggunakan `FILE pointer`. Tetapi, terdapat urutan khusus saat redirect isi dari `.txt` tersebut, yaitu urutannya bergantian dari `.txt` dengan nama angka lalu huruf lalu angka lagi lalu huruf lagi. Lalu semua file `.txt` sebelumnya dihapus.
Contoh urutannya:
- 1.txt
- a.txt
- 2.txt
- b.txt
- dst..

```
void combine_files() {
    FILE *out = fopen("Combined.txt", "w");
    if (!out) {
        perror("Gagal membuat Combined.txt");
        return;
    }
}
```
Membuka file output `Combined.txt` dengan mode write `("w")`. Jika gagal membuat file, tampilkan pesan error dan keluar dari fungsi.

```
for (int i = 1; i <= 9; ++i) { ... }
```
Melakukan iterasi i dari 1 sampai 9 (karena file yang valid hanya dari `1.txt` sampai `9.txt` dan `a.txt` sampai `i.txt`).

```
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
```
- Membuka file `Filtered/i.txt` untuk dibaca.
- Menyalin seluruh isi file ke `Combined.txt` dengan `fread` dan `fwrite`.
- Setelah selesai, file sumber dihapus dengan `remove(path)`.

```
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
```
- Mengambil huruf `'a'` sampai `'i'` dengan kalkulasi `char ch = 'a' + (i-1)`.
- Prosesnya sama dengan file angka: buka → baca → tulis → hapus.

```
{
...
fclose(out);
    printf("Combined.txt berhasil dibuat!\n");
}
```
Setelah semua file disatukan, tutup `Combined.txt` dan tampilkan pesan sukses.

bash:
```
./action --Combine
```
### D. Decode the file
Karena isi `Combined.txt` merupakan string yang random, maka decode menggunakan Rot13 string tersebut dan letakkan hasil dari string yang telah di-decode tadi kedalam file bernama `Decoded.txt`.

```
char rot13(char c) {
    if ('a' <= c && c <= 'z') return 'a' + (c - 'a' + 13) % 26;
    if ('A' <= c && c <= 'Z') return 'A' + (c - 'A' + 13) % 26;
    return c;
}
```
Isi dari fungsi ini:
- Memutar huruf alfabet sebanyak 13 huruf.
- ROT13 adalah metode substitusi sederhana: `'a'` jadi `'n'`, `'b'` jadi `'o'`, ..., `'n'` jadi `'a'`, dst.
- Huruf besar dan kecil diperlakukan berbeda, karakter selain huruf tidak diubah.

```
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
```
Penjelasan fungsi `decode_file()`:
- `fopen("Combined.txt", "r")` 	Membuka file hasil gabungan untuk dibaca
- `fopen("Decoded.txt", "w")`	Membuka (atau membuat) file hasil decoding
- `fgetc()` & `fputc()`	Membaca 1 karakter, memproses dengan rot13(), lalu menulis ke output
- `fclose()`	Menutup file setelah proses selesai

bash:
```
./action --Decode
```

![Password](assets/hasil.png)

### E. Password Check
Input password hasil decode ke web yang sudah disediakan

password: BewareOfAmpy

![Password Check](assets/password_check.png)

## Soal_2

## Soal_3

## Soal_4


