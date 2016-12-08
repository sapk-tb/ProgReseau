#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>


#define BUFSIZE 512

void fin_fils(int n) {
    int status;
    int fils = wait(&status);
    printf("Fils numero: %d\n", fils);

    if (WIFEXITED(status))
        printf("termine sur exit(%d)\n", WEXITSTATUS(status));

    if (WIFSIGNALED(status))
        printf("termine sur signal %d\n", WTERMSIG(status));

    //exit(EXIT_SUCCESS); /* pour terminer le pere */
}

enum TypeFichier {
    NORMAL, REPERTOIRE, ERREUR
};

const char* OK200 = "HTTP/1.1 200 OK\r\n\r\n";
const char* ERROR403 = "HTTP/1.1 403 Forbidden\r\n\r\nAccess denied\r\n";
const char* ERROR404 = "HTTP/1.1 404 Not Found\r\n\r\nFile or directory not found\r\n";
const char* ERROR500 = "HTTP/1.1 500 Internal Server Error\r\n\r\nInternal Server Error\r\n";
const char* ERROR501 = "HTTP/1.1 501 Not Implemented\r\n\r\nNot Implemented\r\n";

/* Fonction typeFichier()
 * argument: le nom du fichier
 * rend une valeur de type enumeration delaree en tete du fichier
 * NORMAL, ou REPERTOIRE ou ERRREUR
 */
enum TypeFichier typeFichier(char *fichier) {
    struct stat status;
    int r;

    r = stat(fichier, &status);
    if (r < 0)
        return ERREUR;
    if (S_ISREG(status.st_mode))
        return NORMAL;
    if (S_ISDIR(status.st_mode))
        return REPERTOIRE;
    /* si autre type, on envoie ERREUR (a fixer plus tard) */
    return ERREUR;
}


/* envoiFichier()
 * Arguments: le nom du fichier, la socket
 * valeur renvoyee: true si OK, false si erreur
 */
#define BUSIZE 1048;

bool envoiFichier(char *fichier, int soc) {
    printf("Send file %s\n", fichier);
    //*
    //int fd;
    char buf[BUFSIZE];
    //*/
    /* A completer.
     * On peut se poser la question de savoir si le fichier est
     * accessible avec l'appel systeme access();
     * Si oui, envoie l'entete OK 200 puis le contenu du fichier
     * Si non, envoie l'entete ERROR 403
     *
     * Note: le fichier peut etre plus gros que votre buffer,
     * de meme il peut etre plus petit...
     */

    int rval;
    /* Check file existence. */
    rval = access(fichier, F_OK);
    if (rval == 0)
        printf("%s exists\n", fichier);
    else {
        if (errno == ENOENT) {
            printf("%s does not exist\n", fichier);
            return false;
            //write(soc, ERROR404, strlen(ERROR404));
        } else if (errno == EACCES) {
            printf("%s is not accessible\n", fichier);
            write(soc, ERROR403, strlen(ERROR403));
            sprintf(buf, "<html><title>403 Forbidden</title><body>Access denied</body></html>");
            write(soc, buf, strlen(buf));
            return true;
        }
        return 0;
    }

    /* Check read access. */
    rval = access(fichier, R_OK);
    if (rval == 0)
        printf("%s is readable\n", fichier);
    else {
        printf("%s is not readable (access denied)\n", fichier);
        write(soc, ERROR403, strlen(ERROR403));
        sprintf(buf, "<html><title>403 Forbidden</title><body>Access denied</body></html>");
        write(soc, buf, strlen(buf));
        return true;
    }

    //Lecture du fichier
    FILE *handler = fopen(fichier, "r");
    if (handler) {
        //TODO send data
        write(soc, OK200, strlen(OK200));
        // Seek the last byte of the file
        fseek(handler, 0, SEEK_END);
        // Offset from the first to the last byte, or in other words, filesize
        ssize_t f_size = ftell(handler);
        // go back to the start of the file
        rewind(handler);

        // Allocate a string that can hold it all
        char *buffer = (char*) malloc(sizeof (char) * (f_size + 1));

        // Read it all in one operation
        ssize_t r_size = fread(buffer, sizeof (char), f_size, handler);

        // fread doesn't set it so put a \0 in the last position
        // and buffer is now officially a string
        buffer[f_size] = '\0';

        if (f_size != r_size) {
            // Something went wrong, throw away the memory and set
            // the buffer to NULL
            free(buffer);
            buffer = NULL;
            write(soc, ERROR500, strlen(ERROR500));
            sprintf(buf, "<html><title>500 Internal Server Error</title><body>500 Internal Server Error</body></html>");
            write(soc, buf, strlen(buf));
            return true;
        }
        //Write data to socket            
        write(soc, buffer, strlen(buffer));
        // Always remember to close the file.
        fclose(handler);
        return true;
    }
    return false;
}

/* envoiRep()
 * Arguments: le nom du repertoire, la socket
 * valeur renvoyee: true si OK, false si erreur
 */
bool envoiRep(char *rep, int soc) {
    printf("Send folder list %s\n", rep);
    DIR *dp;
    struct dirent *pdi;
    char buf[1024];

    dp = opendir(rep);
    if (dp == NULL)
        return false;

    write(soc, OK200, strlen(OK200));
    sprintf(buf, "<html><title>Repertoire %s</title><body><ul>", rep);
    write(soc, buf, strlen(buf));

    //char cwd[1024];
    //getcwd(cwd, sizeof(cwd));

    while ((pdi = readdir(dp)) != NULL) {
        /* A completer
         * Le nom de chaque element contenu dans le repertoire est retrouvable a
         * l'adresse pdi->d_name. Faites man readdir pour vous en convaincre.
         * Dans un premier temps, on se contentera de la liste simple.
         * Dans une petite amelioration on poura prefixer chaque element avec
         * l'icone folder ou generic en fonction du type du fichier.
         * (Tester le nom de l'element avec le chemin complet.) */
        char nom[1024];
        char buf2[2048];
        strcpy(nom, rep);
        strcat(nom, "/");
        strcat(nom, pdi->d_name); //TODO pass .. and . ?
        sprintf(buf2, "<li><a href='%s'>%s</a></li>\n\r", nom, nom); //TODO add img and complete pwd
        write(soc, buf2, strlen(buf2));
    }
    write(soc, "</ul></body></html>\n\r", strlen("</body></html>\n\r"));
    return true;
}

void communication(int soc, struct sockaddr *from, socklen_t fromlen) {
    char buf[BUFSIZE];
    ssize_t nread;
    char host[NI_MAXHOST];
    bool result;
    char *pf;

    enum op {
        GET, PUT
    } operation;

    /* Reconnaissance de la machine cliente */
    int s = getnameinfo((struct sockaddr *) from, fromlen,
            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    if (s == 0)
        printf("Debut avec client '%s'\n", host);
    else
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));
    /* Eventuellement, inserer ici le code pour la reconnaissance de la
     * machine appelante */


    /* Reconnaissance de la requete */
    nread = read(soc, buf, BUFSIZE);
    if (nread > 0) {
        if (strncmp(buf, "GET", 3) == 0)
            operation = GET;
    } else {
        perror("Erreur lecture socket");
        return;
    }

    switch (operation) {
        case PUT:
            write(soc, ERROR501, strlen(ERROR501));
            sprintf(buf, "<html><title>501 Not Implemented</title><body>Method Not Implemented</body></html>");
            write(soc, buf, strlen(buf));
            break;
        case GET:
            pf = strtok(buf + 4, " ");
            /* On pointe alors sur le / de "GET /xxx HTTP...
             * strtok() rend l'adresse du premier caractere
             * apres l'espace et termine le mot par '\0'
             */
            pf++; /* pour pointer apres le slash */
            /* pf pointe sur le nom du fichier suivant le / de la requete.
             * Si la requete etait "GET /index.html ...", alors pf pointe sur
             * le "i" de "index.html"
             */
            /* si le fichier est un fichier ordinaire, on l'envoie avec la fonction
             * envoiFichier().
             * Si c'est un repertoire, on envoie son listing avec la fonction
             * envoiRep().
             * Vous pouvez utiliser la fonction typeFichier() ci-dessous pour tester
             * le type du fichier.
             */

            /************ A completer ici**********/
            switch (typeFichier(pf)) {
                case NORMAL:
                    result = envoiFichier(pf, soc);
                    break;
                case REPERTOIRE:
                    result = envoiRep(pf, soc);
                    break;
                default:
                case ERREUR://Default 
                    result = false;
            }
            if (!result) {

                write(soc, ERROR404, strlen(ERROR404));
                sprintf(buf, "<html><title>404 Not Found</title><body>File or directory not found</body></html>");
                write(soc, buf, strlen(buf));
                break;
            }

    }

    close(soc);
    printf("Closing socket\n");
}

int main(int argc, char **argv) {
    int sfd, s, ns;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    struct sockaddr_storage from;
    socklen_t fromlen;

    if (argc != 2) {
        printf("Usage: %s  port_serveur\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Inserer ici le code d'un serveur TCP concurent */

    /* Construction de l'adresse locale (pour bind) */
    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_INET6; /* Force IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Stream socket */
    hints.ai_flags = AI_PASSIVE; /* Adresse IP joker */
    hints.ai_flags |= AI_V4MAPPED | AI_ALL; /* IPv4 remapped en IPv6 */
    hints.ai_protocol = 0; /* Any protocol */

    s = getaddrinfo(NULL, argv[1], &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() retourne une liste de structures d'adresses.
       On essaie chaque adresse jusqu'a ce que bind(2) reussisse.
       Si socket(2) (ou bind(2)) echoue, on (ferme la socket et on)
       essaie l'adresse suivante. cf man getaddrinfo(3) */

    for (rp = result; rp != NULL; rp = rp->ai_next) {

        /* Creation de la socket */
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); //Maybe not used family but whatever
        if (sfd == -1)
            continue;

        /* Association d'un port a la socket */
        int r = bind(sfd, rp->ai_addr, rp->ai_addrlen);
        if (r == 0)
            break; // Succes
        //*/
        close(sfd);
    }

    if (rp == NULL) { /* Aucune adresse valide */
        perror("bind");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(result); /* Plus besoin */

    /* Positionnement de la machine a etats TCP sur listen */
    if (listen(sfd, 100) == 0) {
        printf("listen ok!\n");
    } else {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        printf("Start of listening loop\n");
        /* Acceptation de connexions */
        fromlen = sizeof (from);
        memset(&result, 0, sizeof (struct addrinfo));
        ns = accept(sfd, (struct sockaddr *) &from, &fromlen);
        if (ns == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        //communication(ns, (struct sockaddr *) &from, fromlen);
        //* 
        int pid = fork();
        switch (pid) {
            case -1:
                fprintf(stdout, "Erreur à la creation du fils");
                break;
            case 0:
                //Code fils
                communication(ns, (struct sockaddr *) &from, fromlen);
                exit(0);
            default:
                //Code père
                printf("I am your father!\n");
                signal(SIGCHLD, fin_fils);
        }
        //*/
    }
}
