#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFSIZE 512

void communication(int, struct sockaddr *, socklen_t);

void fin_fils(int);

int main(int argc, char **argv) {
    int sfd, s, ns, r;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    struct sockaddr_storage from;
    socklen_t fromlen;
    //char *message = "Message a envoyer: ";

    if (argc != 2) {
        printf("Usage: %s  port_serveur\n", argv[0]);
        exit(EXIT_FAILURE);
    }

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

    //("proto TCP : %d\n", IPPROTO_TCP); //debug
    //printf("hints : %s %d %d %d\n", hints.ai_canonname, hints.ai_family, hints.ai_socktype, hints.ai_protocol); //debug
    for (rp = result; rp != NULL; rp = rp->ai_next) {

        /* Creation de la socket */
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); //Maybe not used family but whatever
        if (sfd == -1)
            continue;

        /* Association d'un port a la socket */
        //printf("rp : %s %s %d %d %d\n", rp->ai_addr->sa_data, rp->ai_canonname, rp->ai_family, rp->ai_socktype, rp->ai_protocol); //debug
        //*
        /*
        struct sockaddr_in sin;
        sin.sin_family = rp->ai_family;
        sin.sin_addr.s_addr = INADDR_ANY; //TODO reuse addr from rp?
        sin.sin_port=htons(argv[1]);
         */
        r = bind(sfd, rp->ai_addr, rp->ai_addrlen);
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
                break;
            default:
                //Code père
                printf("I am your father!\n");
                signal(SIGCHLD,fin_fils);
        }
        //*/
    }
}

void communication(int ns, struct sockaddr *from, socklen_t fromlen) {
    char host[NI_MAXHOST];
    ssize_t nread, nwrite;
    char buf[BUFSIZE];
    /* Reconnaissance de la machine cliente */
    int s = getnameinfo((struct sockaddr *) from, fromlen,
            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    if (s == 0)
        printf("Debut avec client '%s'\n", host);
    else
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

    for (;;) {
        printf("Start interact loop\n");
        nwrite = write(ns, "Message a envoyer: ", strlen("Message a envoyer: "));
        if (nwrite < 0) {
            perror("write");
            close(ns);
            break;
        }
        printf("Message send\n");
        nread = read(ns, buf, BUFSIZE);
        if (nread == 0) {
            printf("Fin avec client '%s'\n", host);
            close(ns);
            break;
        } else if (nread < 0) {
            perror("read");
            close(ns);
            break;
        }
        buf[nread] = '\0';
        printf("Message recu '%s'\n", buf);
    }
}

void fin_fils(int n) {
    int status;
    int fils = wait(&status);
    printf("Fils numero: %d\n", fils);

    if (WIFEXITED(status))
        printf("termine sur exit(%d)\n", WEXITSTATUS(status));

    if (WIFSIGNALED(status))
        printf("termine sur signal %d\n", WTERMSIG(status));

    exit(EXIT_SUCCESS); /* pour terminer le pere */
}
