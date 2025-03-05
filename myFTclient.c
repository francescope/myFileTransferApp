//CLIENT

#include "myFTclient.h"

/**
 * Restituisce il numero di byte disponibili sul dispositivo specificato dal percorso.
 *
 * @param path Il percorso del filesystem di cui si desidera conoscere lo spazio disponibile.
 * @return Il numero di byte disponibili. Restituisce 0 in caso di errore.
 */
unsigned long long int available_bytes(const char *path) 
{
    struct statvfs stat;
    
    // chiamata alla funzione statvfs per ottenere informazioni sul filesystem
    if (statvfs(path, &stat) != 0) {
        // errore, impossibile ottenere le informazioni sul filesystem
        return 0;
    }

    // calcola e restituisce il numero di byte disponibili
    return stat.f_bavail * stat.f_frsize;
}

/**
 * Scrive il contenuto ricevuto dal server su un file locale.
 *
 * @param path - Il percorso del file locale dove scrivere i dati.
 * @param client_sock - Il socket connesso al server dal quale ricevere i dati.
 */
void write_file_in_dir(const char *path, int client_sock) 
{
    ssize_t bytes_received;         // definisce una variabile per memorizzare il numero di byte ricevuti
    char buffer[BUFFER_SIZE];       // definisce un buffer di dimensione BUFFER_SIZE per la lettura dei dati dal socket
    
    // O_WRONLY: apertura in modalità scrittura
    // O_CREAT: crea il file se non esiste
    // O_TRUNC: tronca il file a 0 byte se esiste già
    // 0644: permessi del file (lettura e scrittura per il proprietario, solo lettura per gli altri)
    int file_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    // controlla se il file è stato aperto correttamente
    if (file_fd < 0) {
        fprintf(stderr, "Errore apertura file: %s\n", strerror(errno));    // stampa un messaggio di errore se l'apertura del file fallisce
        return;                                                            // termina la funzione in caso di errore
    }

    // controllo se ho abbastanza memoria per salvare il file

    unsigned long long int bytes_on_device = available_bytes(path); // bytes disponibili nel filesystem

    // gestisco il caso di errore della funzione available_bytes
    if (bytes_on_device == 0) {
        fprintf(stderr, "Errore nel controllo dello spazio di memoria disponibile sul dispositivo: \n");
        close(file_fd);
        return;
    }

    // ciclo per ricevere dati dal socket e scriverli nel file
    while ((bytes_received = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) 
    {
        bytes_on_device -= bytes_received;

        // ERRORE: memoria piena
        if (bytes_on_device < 0){
            fprintf(stderr, "Memoria piena: \n");
            close(file_fd);
            return;
        }

        // scrivo i dati che ricevo dal socket nel file identificato dal file descriptor
        if (write(file_fd, buffer, bytes_received) < 0) {
            fprintf(stderr, "Errore scrittura dati: %s\n", strerror(errno));
            close(file_fd);
            return;
        }
    }

    // controlla se si è verificato un errore durante la ricezione dei dati
    if (bytes_received < 0) {
        fprintf(stderr, "Errore durante la ricezione dei dati: %s\n", strerror(errno));
    }
    
    close(file_fd);
}



/**
 * Divide un percorso di file completo in directory e nome del file.
 *
 * @param input - Il percorso completo del file.
 * @param first_part - Puntatore per memorizzare la directory.
 * @param second_part - Puntatore per memorizzare il nome del file.
 */
void divide_dirpath_from_filename(const char *input, char **first_part, char **second_part) 
{
    // trova l'ultima occorrenza del delimitatore '/'
    const char *last_occurrence = strrchr(input, '/'); //restituisce un puntatore alla posizione dell'ultima occorrenza di un carattere specificato in una stringa

    // gestione del caso in cui il percorso non contiene '/'
    if (last_occurrence == NULL) {
        *first_part = strdup(".");      // se non c'è '/', la directory è corrente ('.')
        *second_part = strdup(input);   // l'intero input è considerato il nome del file
        return;
    }

    // calcola le lunghezze delle due parti
    size_t first_part_length = last_occurrence - input;                 // lunghezza della directory
    size_t second_part_length = strlen(input) - first_part_length - 1;  // lunghezza del nome del file

    // alloca memoria per le due parti
    *first_part = (char *)malloc(first_part_length + 1);        // +1 per il terminatore nullo
    *second_part = (char *)malloc(second_part_length + 1);      // +1 per il terminatore nullo

    // copia la directory nella memoria allocata
    strncpy(*first_part, input, first_part_length);
    (*first_part)[first_part_length] = '\0'; // assicurati che la stringa sia terminata da un carattere nullo

    // copia il nome del file nella memoria allocata
    strcpy(*second_part, last_occurrence + 1);
}



/**
 * Crea una directory se non esiste.
 *
 * @param dir - Il percorso della directory da creare.
 * @return 1 se la directory esiste o è stata creata con successo, altrimenti 0.
 */
int create_dir(const char *dir) 
{
    int successful = 0;             // variabile per tenere traccia del successo dell'operazione
    char *first_part = NULL;        // puntatore alla parte directory del percorso
    char *second_part = NULL;       // puntatore alla parte file del percorso

    divide_dirpath_from_filename(dir, &first_part, &second_part); // divide il percorso in directory e nome del file

    struct stat statbuf; // struttura per memorizzare le informazioni sul file

    // controlla se il path specificato da 'first_part' esiste
    if (stat(first_part, &statbuf) != 0)  // 'stat' ritorna 0 se il path esiste, -1 altrimenti
    {
        //controlla se l'errore è 'ENOENT', che significa che il file o directory non esiste
        if (errno == ENOENT) 
        {
            // la directory non esiste, quindi creiamola
            // tenta di creare la directory con permessi 0777 (lettura, scrittura, esecuzione per tutti)
            if (mkdir(first_part, 0777) == -1) { 
                fprintf(stderr, "Errore nella creazione della directory: %s\n", strerror(errno));
            } else {
                successful =  1;
            }
            
        } else {
            fprintf(stderr, "Errore nel controllo del path: %s\n", strerror(errno));
        }
    } 

    //se il path esiste, controlla se è una directory
    else if (S_ISDIR(statbuf.st_mode)) {
        successful = 1;
    } else {
        fprintf(stderr, "Errore, il path '%s' non si riferisce ad una directory\n", dir);
    }

    free(first_part);
    free(second_part);
    return successful;
}



/**
 * Invia il percorso di un file al server.
 *
 * @param client_sock - Il socket connesso al server.
 * @param path - Il percorso del file da inviare.
 */
void send_filepath(int client_sock, const char *path)
{
    // inizializza un buffer di dimensione BUFFER_SIZE e lo riempie con '\0'. (in questo modo evito di scrivere i primi 5 byte e l'ultimo con '/0')
    char buffer[BUFFER_SIZE] = {0};

    // copia il percorso del file nel buffer a partire dal sesto byte
    // usa strncpy per evitare buffer overflow, assicurandosi di non superare la dimensione del buffer
    strncpy(buffer + 5, path, BUFFER_SIZE - 6);

    // invia il contenuto del buffer al server utilizzando il socket del client
    // calcola la lunghezza del buffer usando strlen per includere solo i byte significativi
    if (send(client_sock, buffer, strlen(path)+6, 0) < 0) {
        // se l'invio fallisce, stampa un messaggio di errore
        fprintf(stderr, "Errore durante l' invio del percorso del file al server: %s\n", strerror(errno));
    } else {
        // se l'invio ha successo, stampa il percorso del file inviato
        printf("CLIENT: Invio del percorso del file '%s' al server\n", path);
    }
}



/**
 * Invia i dati di un file al server.
 *
 * @param fd - Il file descriptor del file da leggere.
 * @param client_sock - Il socket connesso al server.
 */
void send_data(int fd, int client_sock) 
{
    char buffer[BUFFER_SIZE];    // buffer per la lettura dei dati dal file
    ssize_t bytes_read;          // variabile per memorizzare il numero di byte letti dal file

    // legge il contenuto del file e lo invia al server in loop
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) 
    {
        // invia i dati letti al server tramite il socket del client
        if (send(client_sock, buffer, bytes_read, 0) < 0) {
            fprintf(stderr, "Errore durante l' invio dei dati del file al server: %s\n", strerror(errno));
            close(fd);
            return;
        }
    }

    // controlla se si è verificato un errore durante la lettura del file
    if (bytes_read < 0) {
            fprintf(stderr, "Errore durante la lettura del file: %s\n", strerror(errno));
    } else {
            printf("CLIENT: Dati del file inviati con successo al server\n");
    }
}



/**
 * Invia l'opzione selezionata al server.
 *
 * @param client_sock - Il socket connesso al server.
 * @param opz - L'opzione da inviare ('w' per scrivere, 'r' per leggere, 'l' per elencare).
 */
void send_option(int client_sock, const char opz) 
{
    if (send(client_sock, &opz, 1, 0) < 0) {
        fprintf(stderr, "Errore durante l' invio del operazione da effettuare al server: %s\n", strerror(errno));
    } else {
        printf("CLIENT: Opzione '%c' inviata con successo al server\n", opz);
    }
}



/**
 * Funzione che invia il contenuto di un file al server.
 *
 * @param client_sock - Il socket connesso al server.
 * @param from_path - Il percorso del file locale da leggere e inviare al server.
 */
void write_mode(int client_sock, const char *from_path) 
{
    // tentativo di aprire il file locale in modalità di sola lettura
    int file_fd = open(from_path, O_RDONLY);

    if (file_fd < 0) {
        // se l'apertura del file fallisce, stampa un messaggio di errore e termina la funzione
        fprintf(stderr, "Errore durante l' apertura del file: %s\n", strerror(errno));
        return;
    }

    // invia i dati del file al server utilizzando il file descriptor aperto e il socket del client
    send_data(file_fd, client_sock);   
    
    // chiude il file descriptor
    close(file_fd);
}



/**
 * Funzione che riceve un file dal server e lo scrive su disco.
 *
 * @param client_sock - Il socket connesso al server.
 * @param destination_path - Il percorso del file locale dove scrivere i dati ricevuti.
 */
void read_mode(int client_sock, const char *destination_path) 
{
    // crea la directory specificata se non esiste
    int is_dir = create_dir(destination_path);
    
    //se la directory esiste o è stata creata con successo, scrive il file nella directory
    if (is_dir) {
        write_file_in_dir(destination_path, client_sock);
    }
}



/**
 * Funzione che riceve e stampa una lista di file dal server.
 *
 * @param client_sock - Il socket connesso al server.
 */
void list_mode(int client_sock) 
{
    ssize_t bytes_received;
    char buffer[BUFFER_SIZE];

    char* error_message = "ls: cannot access";
    char buffer_errore[18]; //lunghezza del ipotetico messaggio di errore di ls -la

    // continua a ricevere dati finché ci sono dati disponibili dal server
    while ((bytes_received = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {

        strncpy(buffer_errore, buffer, 17);     // copia i primi 17 caratteri da buffer a stringa (ovvero la lunghezza del ipotetico messaggio di errore)
        buffer_errore[17] = '\0';               // assicura che la stringa sia terminata correttamente
        
        if (strcmp(buffer_errore, error_message) == 0) {
            fprintf(stderr, "Errore, questo percorso non specifica una directory valida\n");
            return;
        }
        // scrive i dati ricevuti sullo standard output
        if (write(STDOUT_FILENO, buffer, bytes_received) < 0) {
            fprintf(stderr, "Errore durante la scrittura dei dati sullo stdout: %s\n", strerror(errno));
            return;
        }
    }

    // controlla se c'è stato un errore durante la ricezione dei dati
    if (bytes_received < 0) {
        fprintf(stderr, "Errore nella ricezione dei dati dal server: %s\n", strerror(errno));
    }
}












int main(int argc, char *argv[])
{
    // variabili per memorizzare i parametri di input
    char *server_address = NULL;
    int port = 0;
    char *from_path = NULL;
    char *destination_path = NULL;

    // inizializza la struttura per l'indirizzo del server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;    // famiglia di indirizzi IPv4

    char opz = argv[2][1]; // write/read/list (da -w/-r/-l salvo solo la lettera in modo da passare da string a char)
    
    // validazione dell'opzione
    if (opz != 'w' && opz != 'r' && opz != 'l') {
        fprintf(stderr, "Opzione '%c' non valida. Usa -w per scrittura, -r per lettura, -l per lista\n", opz);
        exit(EXIT_FAILURE); 
    }

    // parsing degli argomenti della riga di comando (comincia da 3 perchè oltre al path e myFTclient specifico anche l'opzione in argv[2])
    for (int i = 3; i < argc; i++) 
    {
        if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            server_address = argv[++i];
        } 

        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
            if (port < 1 || port > 65535) {
                fprintf(stderr, "Porta '%s' non valida. Il valore dovrebbe essere tra 1 e 65535: \n", argv[i]);
                exit(EXIT_FAILURE);
            }
        }

        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            from_path = argv[++i];
        } 
        
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            destination_path = argv[++i];
        }
    }

    // verifica che tutti i parametri necessari siano stati forniti
    if (opz == 'w' || opz == 'r') {
        if (!server_address || port == 0 || !from_path) {
            fprintf(stderr, "Mancano argomenti obbligatori per l' opzione '%c': \n", opz);
            exit(EXIT_FAILURE);
        }
        //versione del comando senza -o
        if (!destination_path) {
            destination_path = strdup(from_path);
            if (!destination_path) {
                fprintf(stderr, "Errore nel allocazione della memoria: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
    }
    
    else if (opz == 'l') {
        if (!server_address || port == 0) {
            fprintf(stderr, "Mancano argomenti obbligatori per l' opzione '%c'\n", opz);
            exit(EXIT_FAILURE);
        }
        else if (!from_path){
            from_path = "";
        }
    }



    // creazione del socket
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        fprintf(stderr, "Errore durante la creazione del socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // imposta il numero di porta del server
    server_addr.sin_port = htons(port);

    // converte l'indirizzo IP passato come argomento in un formato utilizzabile dalla struttura sockaddr_in (binario)
    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Errore, l' indirizzo non è valido o non è supportato: %s\n", strerror(errno));
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    // connessione al server
    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        switch (errno) {
            case ECONNREFUSED:
                fprintf(stderr, "Connessione rifiutata sulla porta '%d'. Nessun servizio in ascolto: %s\n", server_addr.sin_port, strerror(errno));
                break;
            case ETIMEDOUT:
                fprintf(stderr, "Connessione scaduta sulla porta '%d'. Il servizio potrebbe non essere disponibile o c'è un problema di rete: %s\n", port, strerror(errno));
                break;
            case EHOSTUNREACH:
                fprintf(stderr, "Indirizzo IP '%s' non raggiungibile: %s\n", server_address, strerror(errno));
                break;
            default:
                fprintf(stderr, "Errore, connessione '%s' fallita: %s\n", server_address, strerror(errno));
                break;
        }
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    // invia l'opzione al server
    send_option(client_sock, opz);
    
    // invia il percorso del file al server (dove scrivere / da dove leggere / da dove listare)
    if (opz == 'w') {
        send_filepath(client_sock, destination_path); 
    }
    else if (opz == 'r' || opz == 'l') {
        send_filepath(client_sock, from_path); 
    }


    // attende la conferma dal server prima di procedere
    char server_response;
    while (1)
    {
        // riceve la risposta dal server
        if (recv(client_sock, &server_response, 1, 0) > 0) {
            if (server_response == 'T') {
                break;
            }
        } else {
            fprintf(stderr, "Errore nella ricezione della conferma del server che dichiara la sua corretta ricezione\n");
            close(client_sock);
            exit(EXIT_FAILURE);
        }
    }

    // esegue l'operazione corrispondente all'opzione
    switch (opz) {
        case 'w':
            write_mode(client_sock, from_path);
            break;
        case 'r':
            read_mode(client_sock, destination_path);
            break;
        case 'l':
            list_mode(client_sock);
            break;
        default:
            fprintf(stderr, "Errore: Opzione '%c' non valida:\n", opz);
            close(client_sock);
            exit(EXIT_FAILURE);
    }

    // chiude il socket del client
    close(client_sock);
    return 0;
}