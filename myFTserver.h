#ifndef MY_FT_SERVER_H
#define MY_FT_SERVER_H

#include <stdio.h>          // per funzioni di input/output come printf e perror
#include <stdlib.h>         // per funzioni di allocazione memoria e altre utilità come malloc, free, exit
#include <unistd.h>         // per funzioni POSIX come close(), read(), write()
#include <sys/stat.h>       // per utilizzare la funzione stat e mkdir
#include <errno.h>          // per gestire gli errori con errno
#include <sys/socket.h>     // per le definizioni per le operazioni sui socket, come socket(), bind(), listen(), accept()
#include <sys/types.h>      // per i tipi di dati base
#include <arpa/inet.h>      // per funzioni di conversione di indirizzi e gestione socket, come inet_pton()
#include <pthread.h>        // per la gestione dei thread, permettendo la programmazione concorrente con pthread_create(), pthread_join()
#include <netinet/in.h>     // per l'utilizzo delle strutture e costanti per i protocolli internet, come sockaddr_in
#include <fcntl.h>          // per funzioni di controllo dei file descriptor, come open(), O_RDONLY
#include <string.h>         // per funzioni di manipolazione delle stringhe
#include <sys/statvfs.h>    // necessaria per fstatvfs

#define MAX_CLIENTS 10      // definisce il numero massimo di client che possono connettersi contemporaneamente
#define BUFFER_SIZE 1024    // definisce la dimensione del buffer usato per leggere e inviare dati
#define PATH_MAX 4096       // definisce la dimensione del buffer usato per unire ft_root_directory e relative_path


// Struttura per memorizzare le informazioni sul client
typedef struct
{
    struct sockaddr_in address;     // indirizzo del client
    int sockfd;                     // file descriptor della socket del client
    int uid;                        // ID univoco del client
} client_t;


client_t *clients[MAX_CLIENTS];         // array di puntatori ai client connessi
pthread_mutex_t clients_mutex;          // mutex per accesso thread-safe all'array dei client (macro poichè dichiarato come variabile globale) 
int uid_counter;                        // contatore globale per gli UID



// Struttura che mi serve per poter passare tutte le informazioni necessarie a handle_client in un unico argomento
typedef struct {
    client_t *client;
    const char *ft_root_directory;
} client_data_t;

unsigned long long int available_bytes(const char *path);
void add_client(client_t *cl);
void remove_client(int uid);
void send_data(int fd, int client_sock);
void write_file_in_dir(const char *path, int client_sock);
void divide_dirpath_from_filename(const char *path, char **dirpath, char **filename);
int ensure_directory_exists(const char *dirpath);
char* receive_path(client_t *cli);
char* construct_full_path(const char *root_directory, char *relative_path);
int is_ip_reachable(const char *ip_str);
void handle_write(client_t *cli, const char *fullpath);
void handle_read(client_t *cli, const char *fullpath);
void handle_list(client_t *cli, const char *fullpath);
void *handle_client(void *arg);

#endif // MY_FT_SERVER_H