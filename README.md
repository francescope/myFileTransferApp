# myFileTransferApp
Applicazione Client-Server che permette di trasferire file da un client ad un server (scrittura di un file) e, viceversa, da un server ad un client (lettura di un file)

Applicazione client-server che permette di trasferire file (TPC/IP) da un client and un server (scrittura di un file) e, viceversa, da un server ad un client (lettura di un file). L'applicazione è composta da due programmi, un client che effettua richieste di lettura e scrittura di un file, un server che riceve le richieste e gestisce le richieste dei client.

Assumendo che l'applicazione server si chiama myFTserver e l'applicazione client si chiama myFTclient il comportamento è il seguente.

Server
Il comando
myFTserver -a server_address -p server_port -d ft_root_directory

esegue il programma server mettendolo in ascolto su di un determinato indirizzo IP e porta (is_ip_reachable() | AF_INET) ed indicando la directory nella quale andare a scrivere/leggere i file. Se ft_root_directory non esiste deve essere creata (ensure_directory_exists() | create_dir()).

Una volta in esecuzione, il server deve accettare connessioni da uno o piu' client (multithreading) e gestirle concorrentemente (array di puntatori ai client connessi con mutex per evitare race condition). Richieste di scrittura concorrenti sullo stesso file devono essere opportunamente gestite (come la richiesta di creazione concorrente di path con lo stesso nome).
Il programma server deve gestire tutte le eccezioni come ad esempio: richiesta di accesso a file non esistente (per la lettura), errore nel binding su IP e porta, parametri di invocazione del comando errati o mancanti, spazio su disco esaurito (controllo prima di inviare dati per evitare crash), interruzione della connessione con il client.

Client
Il comando 
myFTclient -w -a server_address -p port  -f local_path/filename_local -o remote_path/filename_remote

esegue il programma client, crea una connessione con il server specificato da server_address:port, e scrive il file local_path/filename_local sul server con nome filename_remote e nella directory specificata da remote_path. remote_path avra' root nella directory del server specificata con ft_root_directory

il comando
myFTclient -w -a server_address -p port  -f local_path/filename_local

si comporta come il precedente ma il nome del path remoto e del file remoto sono gli stessi del path e file locale.

il comando 
myFTclient -r -a server_address -p port  -f remote_path/filename_remote -o local_path/filename_local

esegue il programma client, crea una connessione con il server specificato da server_address:port e legge il file specificato da remote_path/filename_remote trasferendolo al programma client che lo scrivera' nella directory local_path assegnando il nome filename_local

il comando
myFTclient -r -a server_address -p port  -f remote_path/filename_remote

si comporta come il precedente ma il nome del path locale e del file locale sono gli stessi del path e file remoto.

il comando
myFTclient -l -a server_address -p port  -f remote_path/

permette al client di ottenere la lista dei file che si trovano in remote_path (effettua sostanzialmente un ls -la remoto). La lista dei file deve essere visualizzata sullo standard output del terminale da cui viene eseguito il programma myFTclient.

Il programma client deve gestire tutte le eccezioni del caso. come ad esempio: parametri di input errati, file remoto non esistente (lettura), spazio di archiviazione insufficiente sul server (scrittura) e sul client, interruzione della connessione con il server
