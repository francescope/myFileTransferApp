#ifndef MY_FT_CLIENT_H
#define MY_FT_CLIENT_H


#include <stdio.h>              // per funzioni di input/output come printf e perror
#include <stdlib.h>             // per funzioni di allocazione memoria e altre utilità
#include <unistd.h>             // per funzioni POSIX come close()
#include <string.h>             // per funzioni di manipolazione delle stringhe come strcpy, strcmp, strlen, ecc.
#include <sys/stat.h>           // per utilizzare la funzione stat e mkdir per gestire le informazioni sui file e le directory
#include <fcntl.h>              // per manipolare file descriptor e usare funzioni come open e fcntl
#include <sys/socket.h>         // per le definizioni e funzioni per le operazioni sui socket come socket, bind, listen, e accept
#include <arpa/inet.h>          // per funzioni di conversione di indirizzi e gestione socket come inet_pton e inet_ntop
#include <errno.h>              // per gestire gli errori con errno e interpretare i codici di errore
#include <sys/statvfs.h>        // necessaria per fstatvfs

#define BUFFER_SIZE 1024        // definisce la dimensione del buffer utilizzato per la lettura e scrittura dei dati

unsigned long long int available_bytes(const char *path);
void write_file_in_dir(const char *path, int client_sock);
void divide_dirpath_from_filename(const char *input, char **first_part, char **second_part);
int create_dir(const char *dir);
void send_filepath(int client_sock, const char *path);
void send_data(int fd, int client_sock);
void send_option(int client_sock, const char opz);
void write_mode(int client_sock, const char *from_path);
void read_mode(int client_sock, const char *destination_path);
void list_mode(int client_sock);


#endif // MY_FT_CLIENT_H