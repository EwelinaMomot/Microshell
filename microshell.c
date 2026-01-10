#define _POSIX_C_SOURCE 200809L 
#include <stdio.h>  
#include <unistd.h>  
#include <string.h>   
#include <stdlib.h>   
#include <fcntl.h>   
#include <sys/stat.h>
#include <libgen.h>   
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <dirent.h> 
#include <errno.h>
#include <signal.h>
#include <termios.h> 


//do obslugi sygnalów
pid_t current_child = -1; 
pid_t stopped_child = -1; 
static pid_t shell_pgid = -1;


void autocomplete(char *buffer, int *len) {
    char *last_space = strrchr(buffer, ' ');

    char *prefix = last_space ? last_space + 1 : buffer;

    DIR *dir = opendir("."); 
    if (!dir) return;

    //struktura typu dirent(dirent opisuje element katalogu)
    struct dirent *entry; 
    char common[256] = {0};
    int matches = 0; 


    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
     
            if (matches == 0) {
                strcpy(common, entry->d_name);
            } else {//jesli to kolejne to zaktualizuj common do ich wspolnego prefixu
                int i = 0;
                while (common[i] && entry->d_name[i] && common[i] == entry->d_name[i])
                    i++;
                common[i] = '\0';
            }
            matches++;
        }

    }

    closedir(dir);//zwalnia zasoby systemowe(aby nie bylo wyciekow zasobow)

 
    if (matches == 1) {
        int prefix_len = strlen(prefix);
        int common_len = strlen(common);

        int to_add = common_len - prefix_len;
        memcpy(buffer + *len, common + prefix_len, to_add);
       
        *len += to_add;
        buffer[*len] = '\0';//Po ostatnim znaku tekstu trzeba wstawic znak kończący string, memcpy nie wie ze operuje na string wiec trzeba recznie zakonczyc

        write(STDOUT_FILENO, common + prefix_len, to_add);
        
    }

    else if (matches > 1) {
        int prefix_len = strlen(prefix);
        int common_len = strlen(common);

        //Jeśli da się coś jeszcze dopisać – dopisz
        if (common_len > prefix_len) {
            int to_add = common_len - prefix_len;

            memcpy(buffer + *len, common + prefix_len, to_add);
            *len += to_add;
            buffer[*len] = '\0';

            write(STDOUT_FILENO, common + prefix_len, to_add);
            return;
        }

        // Jeśli nie da się dopisać – pokaż listę
        write(STDOUT_FILENO, "\n", 1);

        dir = opendir(".");
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
                printf("%s  ", entry->d_name);
            }
        }
        closedir(dir);

        printf("\n> %s", buffer);
        fflush(stdout);
    }

}
//kopia ustawień terminala - potrzebna, żeby na końcu wszystko przywrócić
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); // brak buforowania i echo
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int read_line_with_tab(char *buffer, int maxlen) {
    int len = 0;
    char c;

    enable_raw_mode();

    while (1) {
        if (read(STDIN_FILENO, &c, 1) != 1)
            break;

        // ENTER
        if (c == '\n') {
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        // BACKSPACE
        if (c == 127 && len > 0) {
            len--;
            write(STDOUT_FILENO, "\b \b", 3);
            continue;
        }

        // TAB
        if (c == '\t') {
            buffer[len] = '\0';           
            autocomplete(buffer, &len);
            continue;
        }
        // Ctrl+D(EOF)
        if (c == 4) {
            disable_raw_mode();
            printf("\nKoniec wejścia (EOF). Zamykanie powłoki...\n");
            exit(0);
        }


        // normalny znak
        if (len < maxlen - 1) {
            buffer[len++] = c;
            write(STDOUT_FILENO, &c, 1);
        }
    }

    buffer[len] = '\0';
    disable_raw_mode();
    return len;
}



// Funkcja kopiująca plik
void cp_file(char *source, char *destination) {
    int source_fd = open(source, O_RDONLY);  
    if (source_fd == -1) {
        perror("cp");
        return;
    }

    int dest_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0644);  
    if (dest_fd == -1) {
        perror("cp");
        close(source_fd);  
        return;
    }

    char buffer[1024];
    ssize_t bytes_read;
    while ((bytes_read = read(source_fd, buffer, sizeof(buffer))) > 0) {
        write(dest_fd, buffer, bytes_read);  
    }

    close(source_fd);  
    close(dest_fd);    
}


// Funkcja kopiująca katalog rekurencyjnie
void cp_directory(char *source_dir, char *destination_dir) {
    DIR *dir = opendir(source_dir);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;

    //czytamy zawarttosc katalogu zrodlowego
    while ((entry = readdir(dir)) != NULL) {
        // Ignorujemy "." i ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        //tworzymy pelne sciezki do elementow z katalogu zrodlowego
        char source_path[1024], destination_path[1024];
        snprintf(source_path, sizeof(source_path), "%s/%s", source_dir, entry->d_name);
        snprintf(destination_path, sizeof(destination_path), "%s/%s", destination_dir, entry->d_name);

        struct stat st;

        if (stat(source_path, &st) == 0) {//czy sciezka do elementu jest poprawna
            if (S_ISDIR(st.st_mode)) {
                // Jeśli napotkamy katalog, wywołujemy cp_directory rekurencyjnie
                mkdir(destination_path, 0755);  
                cp_directory(source_path, destination_path);  // Rekurencyjnie kopiujemy katalog
            } else {
                
                cp_file(source_path, destination_path);
            }
        }
    }

    closedir(dir);
}


// Funkcja kopiuje plik lub katalog w zależności od opcji
void cp_command(char *source, char *destination, int recursive, int interactive)
{
    struct stat st;
    char final_dest[1024];  

    if (stat(destination, &st) == 0 && S_ISDIR(st.st_mode)) {
        size_t len = strlen(destination);
        int has_slash = (len > 0 && destination[len - 1] == '/');

        if (has_slash) {
            snprintf(final_dest, sizeof(final_dest),
                     "%s%s", destination, basename(source));
        } else {
            snprintf(final_dest, sizeof(final_dest),
                     "%s/%s", destination, basename(source));
        }
    } else {
        strncpy(final_dest, destination, sizeof(final_dest));
        final_dest[sizeof(final_dest) - 1] = '\0';
    }
   
    // OBSŁUGA -i 

    struct stat dest_stat;
    int dest_exists = (stat(final_dest, &dest_stat) == 0);

    if (dest_exists && interactive) {
        printf("cp: overwrite '%s'? (y/n): ", final_dest);
        fflush(stdout);

        char answer[8];
        if (fgets(answer, sizeof(answer), stdin) == NULL)
            return;

        if (answer[0] != 'y' && answer[0] != 'Y') {
            printf("Pominięto %s\n", final_dest);
            return;
        }
    }

    struct stat source_stat;
    if (stat(source, &source_stat) == 0 && S_ISDIR(source_stat.st_mode)) {

        if (!recursive) {
            fprintf(stderr,
                "cp: -r not specified; omitting directory '%s'\n",
                source);
            return;
        }

        if (mkdir(final_dest, 0755) != 0 && errno != EEXIST) {
            perror("mkdir");
            return;
        }

        cp_directory(source, final_dest);
        printf("Skopiowano katalog %s do %s\n", source, final_dest);
        return;
    }

    cp_file(source, final_dest);
    
}




void mv_command(char *source, char *destination, int interactive, int backup)
{
    struct stat st;
    char final_dest[1024];

    if (stat(destination, &st) == 0 && S_ISDIR(st.st_mode)) {

        size_t len = strlen(destination);
        int has_slash = (len > 0 && destination[len - 1] == '/');

        if (has_slash) {
            snprintf(final_dest, sizeof(final_dest),
                     "%s%s", destination, basename(source));
        } else {
            snprintf(final_dest, sizeof(final_dest),
                     "%s/%s", destination, basename(source));
        }

    } else {
        strncpy(final_dest, destination, sizeof(final_dest));
        final_dest[sizeof(final_dest) - 1] = '\0';
    }

    struct stat dest_stat;
    int dest_exists = (stat(final_dest, &dest_stat) == 0);

    // OBSLUGA -i
    if (dest_exists && interactive) {
        printf("mv: overwrite '%s'? (y/n): ", final_dest);
        fflush(stdout);

        char answer[8];
        if (fgets(answer, sizeof(answer), stdin) == NULL)
            return;

        if (answer[0] != 'y' && answer[0] != 'Y') {
            printf("Anulowano\n");
            return;
        }
    }

    //OBSŁUGA FLAGI -b (BACKUP)

   if (dest_exists && backup) {
    char backup_name[1024];

    size_t len = strlen(final_dest);

    if (len + 1 >= sizeof(backup_name)) {
        fprintf(stderr, "mv: backup name too long\n");
        return;
    }

    memcpy(backup_name, final_dest, len);
    backup_name[len] = '~';
    backup_name[len + 1] = '\0';

    if (rename(final_dest, backup_name) != 0) {
        perror("mv: backup");
        return;
    }
}
   
    // WARIANT 1 rename() (ten sam filesystem)
       
    if (rename(source, final_dest) == 0) {
        printf("Przeniesiono %s -> %s\n", source, final_dest);
        return;
    }

    // WARIANT 2 copy + unlink (różne filesystemy)

    int src_fd = open(source, O_RDONLY);
    if (src_fd == -1) {
        perror("mv");
        return;
    }

    int dest_fd = open(final_dest, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd == -1) {
        perror("mv");
        close(src_fd);
        return;
    }

    char buffer[1024];
    ssize_t bytes_read;

    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        if (write(dest_fd, buffer, bytes_read) == -1) {
            perror("mv");
            close(src_fd);
            close(dest_fd);
            return;
        }
    }

    close(src_fd);
    close(dest_fd);

    if (unlink(source) == -1) {
        perror("mv: unlink");
        return;
    }

    printf("Przeniesiono %s -> %s\n", source, final_dest);
}


void handle_sigint(int sig) {
    (void)sig;

    write(STDOUT_FILENO, "\n", 1);
}





int main() {
      
    char cwd[1024]; 
    char input[1024];  
    char *argv[64]; 
    int argc;
    shell_pgid = getpid();
if (setpgid(shell_pgid, shell_pgid) == -1) {
    perror("setpgid(shell)");
}

// Ignorujemy sygnały job-control dla samego shella
signal(SIGTTOU, SIG_IGN);
signal(SIGTTIN, SIG_IGN);
signal(SIGTSTP, SIG_IGN);

 signal(SIGINT, handle_sigint);   // Ctrl+C

// Przejmujemy terminal (ustawiamy microshell jako foreground)
if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
    perror("tcsetpgrp(shell)");
}
   
    while (1) {
        

        argc = 0;
        char *login = getlogin();
        // Wyświetlamy znak zachęty z aktualnym katalogiem roboczym
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
             if (login != NULL) {
                printf("\033[1;34m[%s]\033[1;35m@%s\033[0m $ ", login, cwd); 
                fflush(stdout);
                } else {
                printf("\033[1;35m[%s]$ \033[0m", cwd); 
                fflush(stdout);
            } 
        } else {
            perror("getcwd() error"); 
            continue;
        }

        // Pobieramy polecenie od użytkownika
        if (read_line_with_tab(input, sizeof(input)) == 0) {
            continue;
        }

        char *token = strtok(input, " \n");
        if (token == NULL) {
            continue; 
        }

        argv[argc++] = token;

        while (argc < 63 && (token = strtok(NULL, " \n")) != NULL) {
            argv[argc++] = token;
        }

        argv[argc] = NULL;        
        
        if (strcmp(argv[0], "cd") == 0) {
            static char prev_cwd[1024];  
            char current_cwd[1024];  
        
            if (getcwd(current_cwd, sizeof(current_cwd)) == NULL) {
                perror("getcwd");
                continue;
            }

            // Obsługa cd bez argumentu (przejście do katalogu domowego)
            if (argc < 2) {
                char *home = getenv("HOME"); 
                if (home != NULL) {
                    if (chdir(home) != 0) {
                        perror("cd");
                    } else {                     
                        strcpy(prev_cwd, current_cwd);
                    }
                } else {
                    printf("Nie można znaleźć katalogu domowego\n");
                }
            }

            // Obsługa "cd ~" 
            else if (strcmp(argv[1], "~") == 0) {
                char *home = getenv("HOME");  
                if (home != NULL) {
                    if (chdir(home) != 0) {
                        perror("cd");
                    } else {
                        strcpy(prev_cwd, current_cwd);
                    }
                } else {
                    printf("Nie można znaleźć katalogu domowego\n");
                }
            }

            // Obsługa "cd -" 
            else if (strcmp(argv[1], "-") == 0) {
                if (prev_cwd[0] != '\0') {  
                    if (chdir(prev_cwd) != 0) {
                        perror("cd");
                    } else {
                        printf("Przeniesiono do katalogu: %s\n", prev_cwd);
                        
                        
                        strcpy(prev_cwd, current_cwd);
                    }
                } else {
                    printf("Brak poprzedniego katalogu\n");
                }
            }

            // Obsługa cd <katalog>
            else {
                if (chdir(argv[1]) != 0) {
                    perror("cd");
                } else {
                  
                    strcpy(prev_cwd, current_cwd);
                }
            }
        }



        else if ( strcmp(argv[0], "help") == 0) {
            
            printf("Microshell - powłoka napisana w C\n");
            printf("Autor: Ewelina Momot\n");
            printf("Dostępne polecenia i ich zaimplementowane funkcje:\n");
            printf("  cd [<katalog> | ~ | -]  - zmiana katalogu roboczego:\n");
            printf("                           - cd <katalog>   - przejście do <katalog>\n");
            printf("                           - cd ~           - przejście do katalogu domowego\n");
            printf("                           - cd -           - powrót do poprzedniego katalogu\n");
            printf("                           - cd             - przejście do katalogu domowego (tak jak cd ~)\n");
            printf("  exit                   - zakończenie działania powłoki\n");
            printf("  help                   - wyświetlenie tej pomocy\n");
            printf("  cp <źródło> <cel>      - kopiowanie pliku z <źródło> do <cel>\n");
            printf("                           - jeśli <cel> jest katalogiem, plik zostanie skopiowany do tego katalogu\n");
            printf("  cp -r <katalog> <cel> - rekursywne kopiowanie katalogu wraz z jego zawartością\n");
            printf("                           - bez flagi -r próba kopiowania katalogu zakończy się błędem\n");
            printf("  cp -i <źródło> <cel>  - interaktywne kopiowanie; przed nadpisaniem pliku docelowego\n");
            printf("                           - wyświetlany jest monit z potwierdzeniem\n");
            printf("                           - brak potwierdzenia powoduje pominięcie kopiowania\n");
            printf("  mv <źródło> <cel>      - przenoszenie lub zmiana nazwy pliku lub katalogu\n");
            printf("                         - jeśli <cel> jest katalogiem, element zostanie przeniesiony do tego katalogu\n");
            printf("                         - możliwe jest przenoszenie wielu plików do jednego katalogu\n");
            printf("  mv -i <źródło> <cel>   - interaktywne przenoszenie; przed nadpisaniem\n");
            printf("                         - wyświetlany jest monit z potwierdzeniem\n");
            printf("  mv -b <źródło> <cel>   - tworzy kopię zapasową nadpisywanego pliku\n");
            printf("                         - kopia zapasowa otrzymuje sufiks '~'\n");
            printf("  <program> [argumenty] - uruchamianie programów znajdujących się w zmiennej PATH\n");
            printf("                         - np. 'ls', 'pwd', 'grep'\n");
            printf("\n");
            printf("Specjalne funkcje:\n");
            printf("  Kolorowy prompt        - Login i ścieżka robocza są wyświetlane w kolorach.\n");
            printf("  EOF (Ctrl+D)           - Naciśnięcie Ctrl+D kończy działanie powłoki i zamyka sesję.\n");
            printf("\n");
            printf("Komunikaty błędów:\n");
            printf("  Jeśli polecenie nie jest dostępne, powłoka wyświetli komunikat:\n");
            printf("    'microshell: command not found: <nazwa_polecenia>'\n");
            printf("\n");
            printf("Dodatkowe informacje:\n");
            printf("  Przekazuję polecenia do systemowych programów w tle,\n");
            printf("  wykorzystując funkcję execvp().\n");
            printf("  Login użytkownika jest wyświetlany w promptcie, jeśli jest dostępny.\n");
            printf("Zarządzanie procesami i sygnały:\n");
            printf("  Ctrl+C                 - wysyła sygnał SIGINT do uruchomionego programu\n");
            printf("                         - powoduje natychmiastowe zakończenie procesu\n");
            printf("                         - microshell nie kończy swojego działania\n");
            printf("  Ctrl+Z                 - wysyła sygnał SIGTSTP i zatrzymuje aktualny proces\n");
            printf("                         - zatrzymany proces może zostać wznowiony\n");
            printf("  fg                     - wznawia ostatnio zatrzymany proces na pierwszym planie\n");
            printf("                         - microshell czeka na jego zakończenie lub ponowne zatrzymanie\n");
            printf("  bg                     - wznawia ostatnio zatrzymany proces w tle\n");
            printf("                         - microshell natychmiast wraca do promptu\n");
            printf("  Autouzupełnianie (TAB) - podpowiadanie nazw plików i katalogów\n");
            printf("                         - TAB uzupełnia nazwę lub wspólny prefiks\n");
            printf("                         - przy wielu dopasowaniach wyświetla listę\n");
            printf("                         - przykład: cd fol<TAB> -> cd folder\n");

            printf("\n");

        }


        else if (strcmp(argv[0], "cp") == 0) {

            int recursive = 0;    // -r
            int interactive = 0; // -i
            int arg_start = 1;

            // Obsługa flag
            while (arg_start < argc && argv[arg_start][0] == '-') {
                if (strcmp(argv[arg_start], "-r") == 0) {
                    recursive = 1;
                } else if (strcmp(argv[arg_start], "-i") == 0) {
                    interactive = 1;
                } else {
                    printf("cp: unknown option %s\n", argv[arg_start]);
                    break;
                }
                arg_start++;
            }

            if (argc - arg_start < 2) {
                printf("cp: missing file operand\n");
                continue;
            }

            char *src = argv[arg_start];
            char *dest = argv[arg_start + 1];

            cp_command(src, dest, recursive, interactive);
        }

        else if (strcmp(argv[0], "mv") == 0) {

            int interactive = 0; // flaga -i
            int backup = 0;      // flaga -b
            int arg_start = 1;   // od którego argumentu zaczynają się pliki

            // Sprawdzamy, czy podano flagę
            if (argc > 1 && argv[1][0] == '-') {
                if (strcmp(argv[1], "-i") == 0) {
                    interactive = 1;
                    arg_start++;
                } else if (strcmp(argv[1], "-b") == 0) {
                    backup = 1;
                    arg_start++;
                }
            }

            if (argc - arg_start < 2) {
                printf("mv: missing file operand\n");
                continue;
            }

            char *dest = argv[argc - 1];

            for (int i = arg_start; i < argc - 1; i++) {
                mv_command(argv[i], dest, interactive, backup);
            }
        }


        else if (strcmp(argv[0], "fg") == 0) {

                if (stopped_child <= 0) {
                    printf("fg: brak zatrzymanego procesu\n");
                    continue;
                }

                printf("Wznawiam proces %d\n", stopped_child);

                // Przekazujemy terminal wznawianemu procesowi
                tcsetpgrp(STDIN_FILENO, stopped_child);

                // Wznawiamy proces
                kill(-stopped_child, SIGCONT);

                int status;
                waitpid(stopped_child, &status, WUNTRACED);

                // Odbieramy terminal z powrotem dla microshella
                tcsetpgrp(STDIN_FILENO, shell_pgid);


                if (WIFSTOPPED(status)) {
                    printf("\nProces ponownie zatrzymany (PID=%d)\n", stopped_child);
                    // zostawiamy stopped_child – nadal jest zatrzymany
                } else {
                    // proces się zakończył
                    stopped_child = -1;
                }
            }

            else if (strcmp(argv[0], "bg") == 0) {

                if (stopped_child <= 0) {
                    printf("bg: brak zatrzymanego procesu\n");
                    continue;
                }

                printf("Wznawiam proces %d w tle\n", stopped_child);

                kill(-stopped_child, SIGCONT);
                
                stopped_child = -1;
            }

        
            else if (strcmp(argv[0], "exit") == 0) {
                printf("Zamykanie microshella...\n");

                exit(0);  // Kończymy program
            }

            else {
                pid_t pid = fork();  // Tworzymy nowy proces

                if (pid < 0) {
                    perror("fork");
                    continue;
                }

                if (pid == 0) {  //dziecko

                    // Tworzymy nową grupę procesów (job)
                    setpgid(0, 0);

                    // Przywrócenie domyślnej obsługi sygnałów w dziecku
                    signal(SIGINT, SIG_DFL);   // Ctrl+C
                    signal(SIGTSTP, SIG_DFL);  // Ctrl+Z

                    execvp(argv[0], argv);

                    // Jeśli execvp się nie powiedzie
                    fprintf(stderr, "microshell: command not found: %s\n", argv[0]);
                    exit(1);
                }
                else {  //rodzic
                    int status;

                    // Ustawiamy dziecko jako lidera swojej grupy procesów
                    setpgid(pid, pid);

                    // Przekazujemy kontrolę terminala procesowi potomnemu
                    tcsetpgrp(STDIN_FILENO, pid);

                    current_child = pid;

                    // Czekamy na zakończenie lub zatrzymanie procesu
                    waitpid(pid, &status, WUNTRACED);

                    // ODBIERAMY kontrolę terminala z powrotem
                    tcsetpgrp(STDIN_FILENO, shell_pgid);


                    current_child = -1;

                    if (WIFSTOPPED(status)) {
                        stopped_child = pid;
                        printf("\nProces zatrzymany (PID=%d)\n", pid);
                    }
                }
}


    }

    return 0;
}

