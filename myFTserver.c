// SERVER

#include "myFTserver.h"

client_t *clients[MAX_CLIENTS];                             // array di puntatori ai client connessi
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;  // mutex per accesso thread-safe all'array dei client (macro poichè dichiarato come variabile globale) 
int uid_counter = 10;                                       // contatore globale per gli UID

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
 * Aggiunge un client all'array dei client connessi.
 * @param cl Puntatore al client da aggiungere.
 */
void add_client(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);     // rimane in attesa che il mutex sia disponibile ed effettua un lock su di esso
    
    // scorre la lista per inserire il nodo in coda
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (!clients[i])        // se la posizione è libera
        {      
            clients[i] = cl;    // aggiunge il client
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);   // sblocca il mutex
}



/**
 * Rimuove un client dall'array dei client connessi.
 * @param uid L'ID univoco del client da rimuovere.
 */
void remove_client(int uid)
{
    pthread_mutex_lock(&clients_mutex);     // rimane in attesa che il mutex sia disponibile ed effettua un lock su di esso
    
    // scorre la lista per cercare e rimuvere il nodo con id specificato in input
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (clients[i]) // se la posizione non è libera
        {  
            if (clients[i]->uid == uid) // se l'UID corrisponde
            {  
                clients[i] = NULL;          // rimuove il client dall'array
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);  // sblocca il mutex
}



/**
 * Invia il contenuto di un file al client tramite una socket.
 * @param fd File descriptor del file da inviare.
 * @param client_sock Socket del client a cui inviare il file.
 * 
 * La funzione utilizza un buffer per leggere il file in blocchi di dati e inviarli attraverso la socket 
 * fino a quando tutto il contenuto del file è stato trasmesso.
 */
void send_data(int fd, int client_sock) 
{
    char buffer[BUFFER_SIZE];   // array di caratteri che funge da buffer temporaneo per i dati letti dal file
    ssize_t bytes_read;         // memorizzare il numero di byte letti dal file in ogni iterazione.

    // ciclo di lettura dal file e invio tramite la socket
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) 
    {
        ssize_t total_sent = 0;         // variabile per tracciare il totale dei byte inviati in ogni iterazione
        
        // ciclo che garantisce che tutti i dati siano inviati, anche se send invia solo una parte
        while (total_sent < bytes_read) 
        {
            // buffer + total_sent = inizio del buffer di dati da inviare
            // bytes_read - total_sent = byte che rimangono da inviare
            ssize_t bytes_sent = send(client_sock, buffer + total_sent, bytes_read - total_sent, 0); 
            
            // gestione degli errori di invio
            if (bytes_sent < 0) 
            {
                fprintf(stderr, "Errore durante l'invio dei dati del file al client: %s\n", strerror(errno));
                close(fd);
                close(client_sock); // chiusura della socket in caso di errore
                return;
            }
            total_sent += bytes_sent; // aggiorna il totale dei byte inviati
        }
    }

    if (bytes_read < 0) {
        fprintf(stderr, "Errore durante la lettura del file: %s\n", strerror(errno));
    }    

    close(fd);  // assicurarsi che il file descriptor venga sempre chiuso alla fine della funzione
}



/**
 * Scrive il contenuto ricevuto da una socket in un file.
 * @param path Il percorso del file dove scrivere i dati.
 * @param client_sock Socket del client da cui ricevere i dati.
 */
void write_file_in_dir(const char *path, int client_sock) 
{
    ssize_t bytes_received;         // variabile per memorizzare il numero di byte ricevuti
    char buffer[BUFFER_SIZE];       // buffer per contenere i dati ricevuti

    // apri il file locale in scrittura, crealo se non esiste, e tronca il file se esiste (qualsiasi contenuto preesistente nel file verrà eliminato prima di scrivere i nuovi dati ricevuti dal client)
    int file_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644); 

    // controlla se il file è stato aperto correttamente
    if (file_fd < 0) {
        fprintf(stderr, "Errore apertura file: %s\n", strerror(errno));
        return;
    }

    // controllo se ho abbastanza memoria per salvare il file

    unsigned long long int bytes_on_device = available_bytes(path); // bytes disponibili nel filesystem

    // gestisco il caso di errore della funzione available_bytes
    if (bytes_on_device == 0) {
        fprintf(stderr, "Errore nel controllo della memoria disponibile sul dispositivo\n");
        close(file_fd);
        return;
    }

    // ciclo per ricevere dati dal socket e scriverli nel file
    while ((bytes_received = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) 
    {
        bytes_on_device -= bytes_received;

        // ERRORE: memoria piena
        if (bytes_on_device < 0){
            printf("SERVER: Memoria piena");
            close(file_fd);
            return;
        }

        // scrivo i dati che ricevo dal socket nel file identificato dal file descriptor
        if (write(file_fd, buffer, bytes_received) < 0) {
            fprintf(stderr, "Errore nella scrittura dei byte nel file: %s\n", strerror(errno));
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
 * Divide il percorso della directory dal nome del file.
 * @param path Il percorso completo da dividere.
 * @param dirpath Puntatore per memorizzare il percorso della directory.
 * @param filename Puntatore per memorizzare il nome del file.
 */
void divide_dirpath_from_filename(const char *path, char **dirpath, char **filename) 
{
    char *last_slash = strrchr(path, '/'); // trova l'ultima occorrenza di '/' nel path

    if (last_slash != NULL) {
        *dirpath = strndup(path, last_slash - path);
        *filename = strdup(last_slash + 1);
    } else {
        *dirpath = strdup("");
        *filename = strdup(path);
    }
}



/**
 * Si assicura che una directory esista, e se non esiste, la crea.
 * @param dirpath Il percorso della directory da verificare o creare.
 * @return 1 se la directory esiste o è stata creata con successo, 0 altrimenti.
 */
int ensure_directory_exists(const char *dirpath) 
{
    struct stat statbuf;                        // struttura per memorizzare le informazioni sul file
    char *path_copy = strdup(dirpath);          // copia del percorso per lavorare su di essa
    char current_path[PATH_MAX] = "";           // percorso temporaneo per la creazione delle cartelle
    char *path_part = strtok(path_copy, "/");   // tokenizzatore per dividere il percorso in parti

    // aggiunge uno slash iniziale se il percorso originale lo aveva
    if (dirpath[0] == '/') {
        strcat(current_path, "/");
    }

    // loop su tutte le parti del percorso
    while (path_part != NULL) {
        
        // aggiorna il percorso corrente
        strcat(current_path, path_part);
        strcat(current_path, "/");

        // controlla se il percorso esiste già
        if (stat(current_path, &statbuf) != 0) 
        {
            // se il percorso non esiste, crea la directory
            if (mkdir(current_path, 0777) == -1) {
                fprintf(stderr, "Errore nella creazione della directory: %s\n", strerror(errno));
                free(path_copy); 
                return 0;                
            }
        } else {
            // se il percorso esiste, assicurati che sia una directory
            if (!S_ISDIR(statbuf.st_mode)) {
                fprintf(stderr, "Errore, il path '%s' non si riferisce a una directory\n", current_path);
                free(path_copy); 
                return 0;
            }
        }

        path_part = strtok(NULL, "/"); // ottiene la parte successiva del percorso
    }

    free(path_copy); // libera la memoria allocata per la copia del percorso
    return 1;        // tutto il percorso è stato creato con successo
}



/**
 * Riceve il percorso inviato dal client attraverso la socket. Pulisce l'input dai caratteri \0 iniziali.
 * @param cli Puntatore al client.
 * @return Il percorso ricevuto o NULL in caso di errore.
 */
char* receive_path(client_t *cli) 
{
    char buffer[BUFFER_SIZE];  // buffer per memorizzare il messaggio ricevuto dal client

    // riceve il messaggio dal client
    int receive = recv(cli->sockfd, buffer, sizeof(buffer) - 1, 0);  // sizeof(buffer) - 1 garantisce spazio per il terminatore di stringa \0

    // se il messaggio è valido
    if (receive > 0) 
    {
        buffer[receive] = '\0';  // aggiunge il terminatore di stringa

        // trova il primo carattere non null nel buffer
        int i = 0;
        while (buffer[i] == '\0' && i < receive) {
            i++;
        }

        // sposta il contenuto significativo all'inizio del buffer
        memmove(buffer, buffer + i, receive - i + 1);  // +1 per includere il terminatore di stringa \0
        receive -= i;  // aggiorna la lunghezza dei dati significativi

        // stampa il percorso ricevuto
        printf("SERVER: Il client %d ha mandato questo percorso -> %s\n", cli->uid, buffer);
    }

    // se il client si disconnette o si verifica un errore nella ricezione
    else if (receive == 0) {
        printf("SERVER: Il client %d si è disconnesso\n", cli->uid);  // messaggio di disconnessione
        return NULL;
    } else {
        fprintf(stderr, "Errore durante la ricezione del percorso: %s\n", strerror(errno));
        return NULL;
    }

    // alloca memoria per il percorso da restituire
    char* path = (char*)malloc((receive + 1) * sizeof(char));  // usa receive + 1 per allocare la dimensione corretta della stringa
    if (path == NULL) {
        fprintf(stderr, "Errore durante l'allocazione di memoria per il percorso: %s\n", strerror(errno));
        return NULL;
    }

    // copia il percorso ricevuto nel buffer allocato
    strncpy(path, buffer, receive + 1);

    return path;
}



/**
 * Costruisce un percorso assoluto combinando una directory di root con un percorso relativo.
 * 
 * @param root_directory La directory di root a cui aggiungere il percorso relativo.
 * @param relative_path Il percorso relativo da aggiungere alla directory di root.
 * 
 * @return Un puntatore a una nuova stringa che rappresenta il percorso completo, 
 *         oppure NULL se uno dei parametri è NULL o se l'allocazione di memoria fallisce.
 */
char* construct_full_path(const char *root_directory, char *relative_path)
{
    if (root_directory == NULL || relative_path == NULL) {
        fprintf(stderr, "Input non valido: root_directory e/o relative_path non possono essere vuoti\n");
        return NULL;
    }

    // calcola la lunghezza necessaria per il percorso completo
    size_t root_len = strlen(root_directory);
    size_t relative_len = strlen(relative_path);

    // aggiungi uno spazio per il terminatore di stringa e un carattere di separazione '/'
    size_t len = root_len + relative_len + 2;

    // alloca memoria per il percorso completo
    char *full_path = (char *)malloc(len);
    if (full_path == NULL) {
        fprintf(stderr, "Errore durante l'allocazione della memoria per il percorso completo: %s\n", strerror(errno));
        return NULL;
    }

    // copia la directory radice nel percorso completo
    strcpy(full_path, root_directory);

    // aggiunge il separatore di directory solo se necessario
    if (root_directory[root_len - 1] != '/') {
        strcat(full_path, "/");
    }

    //se il path relativo inizia per '/' lo taglio onde evitare casi di root//sottodir/file.txt
    while (relative_path[0] == '/') {
        memmove(relative_path, relative_path + 1, strlen(relative_path));
    }

    // concatena il percorso relativo al percorso completo
    strcat(full_path, relative_path);

    return full_path;
}



/**
 * Esegue il comando ping per verificare se un indirizzo IP è raggiungibile.
 * 
 * @param ip_str La stringa dell'indirizzo IP da verificare.
 * @return 1 se l'indirizzo IP è raggiungibile, 0 altrimenti.
 */
int is_ip_reachable(const char *ip_str) 
{
    char command[100];
    snprintf(command, sizeof(command), "ping -c 1 -W 1 %s > /dev/null 2>&1", ip_str);
    // ping -c 1 -W 1 <ip_str>: Esegue il comando ping per inviare un singolo pacchetto ICMP Echo Request (-c 1) con un timeout di 1 secondo (-W 1) all'indirizzo IP specificato.
    // > /dev/null 2>&1: Reindirizza l'output del comando (stdout e stderr) a /dev/null per evitare di visualizzare l'output del comando ping.
    
    int result = system(command); // esegue il comando ping costruito dinamicamente
    return result == 0; // verifica se il valore di ritorno del comando ping è 0, il che indica che il comando è stato eseguito con successo e l'indirizzo IP ha risposto
}





/**
 * Gestisce l'operazione di scrittura ('w') richiesta dal client.
 * 
 * @param cli Il puntatore al client che ha inviato la richiesta.
 * @param fullpath Il percorso completo del file su cui operare.
 */ 
void handle_write(client_t *cli, const char *fullpath) 
{
    char *dirpath = NULL;
    char *filename = NULL;

    divide_dirpath_from_filename(fullpath, &dirpath, &filename);

    int is_dir = ensure_directory_exists(dirpath);   // crea la directory se non esiste
    
    printf("SERVER: Gestisce la scrittura su questo percorso -> %s\n", fullpath);

    // se la directory esiste o è stata creata con successo
    if (is_dir) {
        write_file_in_dir(fullpath, cli->sockfd); // scrivi il file nella directory
        printf("SERVER: Compito eseguito con successo\n");
    }
    free(dirpath);
    free(filename);
}



/**
 * Gestisce l'operazione di lettura ('r') richiesta dal client.
 * 
 * @param cli Il puntatore al client che ha inviato la richiesta.
 * @param fullpath Il percorso completo del file su cui operare.
 */ 
void handle_read(client_t *cli, const char *fullpath) 
{
    int file_fd = open(fullpath, O_RDONLY); // apri il file locale in lettura

    // un valore di file descriptor < 0 indica un errore o una situazione anomala
    if (file_fd < 0) {
        fprintf(stderr, "Errore apertura file: %s\n", strerror(errno));
        return;
    }

    // invia il contenuto del file al client
    send_data(file_fd, cli->sockfd); 
    
    close(file_fd);    // chiude il file
    printf("SERVER: Compito eseguito con successo\n");
}



/**
 * Gestisce l'operazione di lettura ('r') richiesta dal client.
 * 
 * @param cli Il puntatore al client che ha inviato la richiesta.
 * @param fullpath Il percorso completo del file su cui operare.
 */ 
void handle_list(client_t *cli, const char *fullpath)
{
    // creazione del comando per eseguire "ls -la" sul percorso specificato
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "ls -la %s 2>&1", fullpath);  // redirige lo stderr (flusso di errore) allo stdout (flusso standard). Questo permette a popen di catturare sia l'output che gli errori.

    // apertura di un processo per eseguire il comando e ottenere il suo output
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        fprintf(stderr, "Errore nell' apertura del processo che esegue il comando ls -la: %s\n", strerror(errno));
        return;
    }

    // lettura dell'output del comando e invio al client
    char result[BUFFER_SIZE];

    /* fgets legge una riga di testo dall'output del comando, che è gestito dal file pointer fp.
    sizeof(result) specifica la quantità massima di caratteri che fgets può leggere in una volta.
    Il ciclo continua fino a quando ci sono righe da leggere (ovvero, fino a quando fgets non restituisce NULL).*/
    while (fgets(result, sizeof(result), fp) != NULL)
    {
        if (send(cli->sockfd, result, strlen(result), 0) < 0)
        {
            fprintf(stderr, "Errore durante l'invio di dati al client: %s\n", strerror(errno));
            pclose(fp);
            return;
        }
    }
    // chiusura del processo
    if (pclose(fp) == -1) {
        fprintf(stderr, "Errore durante la chiusura del processo: %s\n", strerror(errno));
    } 
    printf("SERVER: Compito eseguito con successo\n");
}



/**
 * Gestisce la comunicazione con il client.
 * 
 * @param arg Il parametro passato al thread, che è un puntatore a client_data_t.
 * @return NULL alla fine dell'esecuzione della funzione.
 * 
 * Questa funzione gestisce la comunicazione con il client identificato da `arg`.
 * Riceve l'operazione richiesta dal client, il percorso relativo del file o directory,
 * costruisce il percorso completo utilizzando la directory di root, e gestisce l'operazione
 * richiesta (scrittura, lettura, elenco).
 * Libera la memoria allocata per le risorse utilizzate.
 */
void *handle_client(void *arg) 
{
    char opz;                   //char per salvare l'opzione richiesta dal client
    char conferma_ricezione;    //char per inviare un carattere al client che gli comunica l'esito del operazione richiesta

    // cast del parametro di tipo void* a client_data_t* e assegnamento parametri
    client_data_t *data = (client_data_t *)arg;    
    client_t *cli = data->client;
    const char *ft_root_directory = data->ft_root_directory;    

    printf("SERVER: Siamo nel thread del client con UID -> %d\n", cli->uid); // log per sapere quale client stiamo gestendo

    // ricezione dell'operazione richiesta dal client
    if (recv(cli->sockfd, &opz, 1, 0) <= 0) {
        fprintf(stderr, "Errore durante la ricezione del operazione richiesta dal client: %s\n", strerror(errno));
        goto cleanup;
    }

    printf("SERVER: Operazione richiesta -> %c \n", opz);  // log per sapere quale operazione è stata richiesta dal client

    char* relative_path = receive_path(cli);             // ricezione del percorso relativo del file o directory

    if (relative_path == NULL) {
        fprintf(stderr, "Errore durante la ricezione del percorso\n");
        goto cleanup;
    }

    // invio della conferma di ricezione dell'operazione e del percorso
    conferma_ricezione = 'T'; // T sta per true
    if (send(cli->sockfd, &conferma_ricezione, 1, 0) <= 0) {
        fprintf(stderr, "Errore durante l'invio della conferma di ricezione al client: %s\n", strerror(errno));
        goto cleanup;
    }

    // costruzione del percorso completo combinando la directory di root con il percorso relativo
    char* fullpath = construct_full_path(ft_root_directory, relative_path); 
    free(relative_path);  
    if (fullpath == NULL) {
        fprintf(stderr, "Errore nella costruzione del percorso completo\n");
        goto cleanup;
    }

    pthread_mutex_lock(&clients_mutex);
    // gestione dell'operazione richiesta dal client
    switch (opz) {
        case 'w':
            handle_write(cli, fullpath);
            break;
        case 'r':
            handle_read(cli, fullpath);
            break;
        case 'l':
            handle_list(cli, fullpath);
            break;
        default:
            fprintf(stderr, "Operazione %c non valida\n", opz);
            break;
    }
    pthread_mutex_unlock(&clients_mutex);
    free(fullpath);  // libera la memoria allocata per il percorso completo

cleanup:
    close(cli->sockfd);         // chiude la socket del client
    remove_client(cli->uid);    // rimuove il client dall'array
    free(cli);                  // libera la memoria allocata per il client
    free(data);                 // libera la memoria allocata per la struttura
    return NULL;
}















int main(int argc, char* argv[]) 
{
    int server_socket, new_socket;          // dichiarazione dei file descriptor per la socket del server e per la nuova connessione

    char *ft_root_directory = NULL;         // puntatore per memorizzare la directory root del file transfer
    int port = 0;                           // porta su cui il server ascolterà

    struct sockaddr_in server_address;      // struttura per l'indirizzo del server
    server_address.sin_family = AF_INET;    // assegna la famiglia di indirizzi IPv4

    // parsing degli argomenti della riga di comando
    for (int i = 2; i < argc; i++) 
    {
        // controlla se l'argomento corrente è "-a" e se c'è un valore successivo 
        if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) 
        {
            int is_reac = is_ip_reachable(argv[i+1]);
            // converte l'indirizzo IP da stringa a binario e lo memorizza
            if ((inet_pton(AF_INET, argv[++i], &server_address.sin_addr) <= 0) || !is_reac) {
                fprintf(stderr, "Errore, indirizzo non valido: %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
        }
        
        // controlla se l'argomento corrente è "-p" e se c'è un valore successivo 
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
        {
            // converte la porta da stringa a intero 
            port = atoi(argv[++i]);

            if (port < 1 || port > 65535) {
                fprintf(stderr, "Porta '%s' non valida. Il valore dovrebbe essere tra 1 e 65535: \n", argv[i]);
                exit(EXIT_FAILURE);
            }
            server_address.sin_port = htons(port); // converte la porta in formato di rete
        }
        
        // controlla se l'argomento corrente è "-d" e se c'è un valore successivo
        else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {  
            ft_root_directory = argv[++i];  // assegna la directory root del file transfer
        }
    }

    // check per la validità della directory root
    if (ft_root_directory && !ensure_directory_exists(ft_root_directory)) {
        fprintf(stderr, "Errore durante il controllo del esistenza della root directory\n");
        exit(EXIT_FAILURE);
    }

    // creazione della socket del server
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Errore durante la creazione della socket del server: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // binding dell'indirizzo alla socket
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        fprintf(stderr, "Errore durante il binding: %s\n", strerror(errno));
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // messa in ascolto della socket
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        fprintf(stderr, "Errore listen: %s\n", strerror(errno));
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("SERVER: Ascolto sulla porta -> %d\n\n", port); // stampa la porta su cui il server è in ascolto


    while (1) 
    {
        struct sockaddr_in client_address;                  // struttura per memorizzare l'indirizzo del client
        socklen_t client_len = sizeof(client_address);      // lunghezza della struttura dell'indirizzo del client
  
        // accetta una nuova connessione
        if ((new_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_len)) < 0) {
            fprintf(stderr, "\nErrore durante l' accettazione del client: %s\n", strerror(errno));
            continue;    // continua ad accettare altre connessioni se c'è un errore
        } else {
            printf("\nSERVER: Il server accetta il client con successo\n");
        }

        // allocazione memoria per la struttura client_data_t e client_t
        client_data_t *cli = (client_data_t *)malloc(sizeof(client_data_t));
        cli->client = (client_t *)malloc(sizeof(client_t)); 

        cli->ft_root_directory = ft_root_directory;  // assegna la directory root del file transfer al client

        cli->client->address = client_address;       // assegna l'indirizzo del client
        cli->client->sockfd = new_socket;            // assegna il file descriptor della nuova connessione
        cli->client->uid = uid_counter++;            // assegna un UID univoco al client e incrementa il contatore
        
        add_client(cli->client);              // aggiunge il client all'array dei client connessi

        // crea un nuovo thread per gestire la comunicazione con il client
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void *)cli) != 0) {
            fprintf(stderr, "Errore creazione del thread: %s\n", strerror(errno));
            close(new_socket);
            free(cli);
        }
        /* indica che il thread tid non deve mai essere unito con PTHREAD_JOIN. Le risorse di tid saranno quindi 
        liberate immediatamente quando termina, invece di attendere che un altro thread esegua PTHREAD_JOIN su di esso.*/
        pthread_detach(tid);
    }

    close(server_socket);
    return 0;
}