/* Edited by: Charuvit Wannissorn 1000149341 (12/4/12)
 * Description: the program acts as a server and a client, communicating with
 *              other instances of itself by sending a stdin-input message, or
 *              receive a message and choose to relay or display it according
 *              to whether the message is originated by the program itself and
 *              whether the "relaymax" has been reached.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/signal.h>
#include <netdb.h>
#include "peer.h"
#include "parsemessage.h"
#include "util.h"

#define DEFPORT 1234  /* default both for connecting and for listening */
#define MAXMESSAGE 256 /* max size of a message, not including '\n' nor '\0' */

unsigned long ipaddr = 0;  /* 0 means not known yet */
int myport = DEFPORT;
int relaymax = 10;
int verbose = 0;
int listenfd;

int main(int argc, char **argv)
{
    int c;
    struct peer *p, *nextp;

    extern void doconnect(unsigned long ipaddr, int port);
    extern unsigned long hostlookup(char *host);

    /* added functions */
    extern void setup();
    extern void displayMessage(char *s);
    extern void findNewPeer(char *s);
    extern char *myreadline(struct peer *p);
    extern void read_and_process(struct peer *p);

    while ((c = getopt(argc, argv, "p:c:v")) != EOF) {
        switch (c) {
        case 'p':
            if ((myport = atoi(optarg)) == 0) {
                fprintf(stderr, "%s: non-numeric port number\n", argv[0]);
                return(1);
            }
            break;
        case 'c':
            relaymax = atoi(optarg);
            break;
        case 'v':
            verbose = 1;
            break;
        default:
            fprintf(stderr, "usage: %s [-p port] [-c relaymax] [-v] [host [port]]\n", argv[0]);
            return(1);
        }
    }

    if (optind < argc) {
        optind++;
        doconnect(hostlookup(argv[optind - 1]),
                  (optind < argc) ? atoi(argv[optind]) : DEFPORT);
    }

/*------------------------- Added Code -----------------------------*/
    setup();

    while (1) {
        int maxfd = listenfd;
        fd_set fdlist;

        FD_ZERO(&fdlist);
        FD_SET(listenfd, &fdlist);
        FD_SET(0, &fdlist);

        for (p = top_peer; p; p = p->next) {
            FD_SET(p->fd, &fdlist);

            if (maxfd < p->fd)
                maxfd = p->fd;
        }

        if (select(maxfd + 1, &fdlist, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(1);
        }

        /* case 1: a new connection is connecting to the server */
        if (FD_ISSET(listenfd, &fdlist)) {
            int clientfd; /* client connecting to us, the server */
            struct sockaddr_in q;
            socklen_t len = sizeof q;
            char YAKline[100];

            if ((clientfd = accept(listenfd,(struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                return(1);
            }

            /* add the new peer */
            if (add_peer(ntohl(q.sin_addr.s_addr), 0) == NULL) {
                perror("add_peer");
                exit(1);
            }

            top_peer->fd = clientfd;
            top_peer->YAKlineReceived = 0;

            printf("new connection from %s, fd %d\n",
                   format_ipaddr(top_peer->ipaddr), top_peer->fd);

            /* format and send the YAK line according to PROTOCOL */
            sprintf(YAKline, "YAK %s\r\n", format_ipaddr(top_peer->ipaddr));

            if (write(top_peer->fd, YAKline, strlen(YAKline)) < 0) {
                perror("write");
                exit(1);
            }

        /* case 2: stdin input from the user */
        } else if (FD_ISSET(0, &fdlist)) {
            char msg[1000], msg2[1200];

            if (fgets(msg, sizeof(msg), stdin) == NULL)
                exit(1);

            /* case 2A: print peer list if input is a blank line: */
            if (msg[0] == '\n') {
                printf("%d peer(s)\n", count_peers());

                for (p = top_peer ; p; p = p->next)
                    printf("peer on fd %d is on port %d of %s\n",
                           p->fd, p->port, format_ipaddr(p->ipaddr));

                printf("end of peer list\n");

            /* case 2B: send the message to a random peer */
            } else if (count_peers() == 0) {
                printf ("No peer to send to!\n");
            } else {
                struct peer *randomP = random_peer();
                char *end;

                if ((end = strchr(msg, '\n')))
                    *end = '\0';

                sprintf(msg2, "%s,%d;;%s\r\n",
                        format_ipaddr(ipaddr), myport, msg);

                if (verbose)
                    printf("Sending to %s %d: %s",
                           format_ipaddr(randomP->ipaddr), randomP->port, msg2);

                if (write(randomP->fd, msg2, strlen(msg2)) < 0) {
                    perror("write");
                    exit(1);
                }
            }

         /* case 3: a peer is transmitting a message to the server */
         } else {
            for (p = top_peer ; p; p = nextp) {
                nextp = p->next; /* in case we remove this peer b/c of error */

                if (FD_ISSET(p->fd, &fdlist))
                    read_and_process(p);
            }
        }
    }

    return(0);
}

/* server setup: socket(), bind(), listen() */
void setup() {
    struct sockaddr_in r;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(myport);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
}

/* connect to a new peer via their ip address and port number*/
void doconnect(unsigned long ipaddr, int port) {
    struct sockaddr_in r;
    char YAKline[100];

    printf("Connecting to %s, port %d\n", format_ipaddr(ipaddr), port);

    if (add_peer(ipaddr, port) == NULL) {
        perror("add_peer");
        exit(1);
    }

    top_peer->YAKlineReceived = 0;

    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = htonl(ipaddr);
    r.sin_port = htons(port);

    if ((top_peer->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    if (connect(top_peer->fd, (struct sockaddr *)&r, sizeof r) < 0) {
        perror("connect");
        exit(1);
    }

    /* send the YAK line right after connection */
    sprintf(YAKline, "YAK %s %d\r\n", format_ipaddr(top_peer->ipaddr), myport);

    if (write(top_peer->fd, YAKline, strlen(YAKline)) < 0) {
        perror("write");
        exit(1);
    }
}

unsigned long hostlookup(char *host)
{
    struct hostent *hp;
    struct in_addr a;

    if ((hp = gethostbyname(host)) == NULL) {
        fprintf(stderr, "%s: no such host\n", host);
        exit(1);
    }

    if (hp->h_addr_list[0] == NULL || hp->h_addrtype != AF_INET) {
        fprintf(stderr, "%s: not an internet protocol host name\n", host);
        exit(1);
    }

    memcpy(&a, hp->h_addr_list[0], hp->h_length);
    return(ntohl(a.s_addr));
}

int analyze_banner(char *s, struct peer *p)
{
    unsigned long a, b, c, d, newipaddr;
    int numfields;
    int newport;

    numfields = sscanf(s, "%lu.%lu.%lu.%lu %d", &a, &b, &c, &d, &newport);

    if (numfields < 4) {
        fprintf(stderr, "'%s' does not begin with an IP address\n", s);
        return(-1);
    }

    newipaddr = (a << 24) | (b << 16) | (c << 8) | d;

    if (ipaddr == 0) {
        ipaddr = newipaddr;
        printf("I've learned that my IP address is %s\n",
               format_ipaddr(ipaddr));
    } else if (ipaddr != newipaddr) {
        fprintf(stderr,
"fatal error: I thought my IP address was %s, but newcomer says it's %s\n",
                format_ipaddr(ipaddr), s);
        exit(1);
    }

    if (numfields > 4) {
        if (p->port == 0) {
            struct peer *q = find_peer(p->ipaddr, newport);

            if (q == NULL) {
                p->port = newport;
                printf(
"I've learned that the peer on fd %d's port number is %d\n",
                            p->fd, p->port);
            } else {
                printf(
"fd %d's port number is %d, so it's a duplicate of fd %d, so I'm dropping it.\n",
                            p->fd, newport, q->fd);
                delete_peer(p);
            }

        } else if (p->port != newport) {
                printf(
"I'm a bit concerned because I thought the peer on fd %d's port number was %d, but it says it's %d\n",
                    p->fd, p->port, newport);
        }
    }

    return(0);
}

char *myreadline(struct peer *p) {
    int nbytes;

    /* move the leftover data to the beginning of buf */
    if (p->bytes_in_buf && p->nextpos)
        memmove(p->buf, p->nextpos, p->bytes_in_buf);

    /* If we've already got another whole line, return it without a read() */
    if ((p->nextpos = extractline(p->buf, p->bytes_in_buf))) {
        p->bytes_in_buf -= (p->nextpos - p->buf);

        return(p->buf);
    }

    /* Try a read--we never fill the buffer, so there's room for a \0 */
    nbytes = read(p->fd, p->buf + p->bytes_in_buf,
                  sizeof p->buf - p->bytes_in_buf - 1);

    if (nbytes <= 0) {
        if (nbytes < 0) perror("read()");

        /* nbytes == 0, see EOF, disconnect */
        printf("Disconnecting fd %d, ipaddr %s, port %d\n",
               p->fd, format_ipaddr(ipaddr), p->port);
        fflush(stdout);
        delete_peer(p);

    } else {

        p->bytes_in_buf += nbytes;

        /* So, _now_ do we have a whole line? */
        if ((p->nextpos = extractline(p->buf, p->bytes_in_buf))) {
            p->bytes_in_buf -= (p->nextpos - p->buf);

            return(p->buf);
        }

        /*
         * Don't do another read(), to avoid the possibility of blocking.
         * However, if we've hit the maximum message size, call it all a line.
         */
        if (p->bytes_in_buf >= MAXMESSAGE) {
            p->buf[p->bytes_in_buf] = '\0';
            p->bytes_in_buf = 0;
            p->nextpos = NULL;

            return(p->buf);
        }
    }

    /* If we got to here, we don't have a full input line yet. */
    return(NULL);
}

/* display the message and its history */
void displayMessage(char *s) {
    struct ipaddr_port *r;

    printf("The history of the message was:\n");

    /* print all IP addresses & port numbers from the begining */
    setparsemessage(s);

    while ((r = getparsemessage())) {
        if (r->ipaddr == ipaddr && r->port == myport)
            printf("    you\n");
        else
            printf("    IP address %lu.%lu.%lu.%lu, port %d\n",
                (r->ipaddr >> 24) & 255,
                (r->ipaddr >> 16) & 255,
                (r->ipaddr >> 8) & 255,
                 r->ipaddr & 255,
                 r->port);
        }

    printf("And the message was: %s\n", getmessagecontent());
}

/* find a new peer: check each (ip addr, port number) in the message history */
void findNewPeer(char *s) {
    struct ipaddr_port *r;

    setparsemessage(s);

    while ((r = getparsemessage())) {
         if (find_peer(r->ipaddr, r->port) == NULL &&    /* not in peer list */
             !(r->ipaddr == ipaddr && r->port == myport)) {/* not the server */
             printf("The message mentions %s %d, which I haven't heard of!\n",
                         format_ipaddr(r->ipaddr), r->port);
             doconnect(r->ipaddr, r->port);
         }
    }
}

void read_and_process(struct peer *p) {
    char *s = myreadline(p);
    struct ipaddr_port *r;

    if (!s) return; /* s == NULL (not an entire line yet) */

    /* case 1: analyze the YAK line from a peer */
    if (p->YAKlineReceived != 1 && strncmp(s, "YAK ",4) == 0) {
        p->YAKlineReceived = 1;
        analyze_banner(s+4, p);

    /* case 2: process the message from a peer */
    } else {
        int count = 0; //number of peers in the message history

        if (verbose)
            printf("Received message to evaluate: %s\n", s);

        setparsemessage(s);
        while ((r = getparsemessage()) &&
               !(r->ipaddr == ipaddr && r->port == myport))
            count++;

        /* case 2A: relaymax reached: display */
        if (count >= relaymax) {
            printf("Here's a message (relaymax reached, so we won't relay):\n");
            displayMessage(s);

        /* case 2B: originated by me AND relaymax not reached: display */
        } else if (r && getparsemessage() == NULL) {
            printf("Here's a message from you!\n");
            displayMessage(s);

        /* case 2C: not originated by me AND relaymax not reached: relay */
        } else {
            char msg2[1200];
            struct peer *randomP = random_peer();

            sprintf(msg2, "%s,%d;%s\r\n", format_ipaddr(ipaddr), myport, s);

            if (verbose)
                printf("relaying to %s, port %d\n",
                       format_ipaddr(randomP->ipaddr), randomP->port);

            if (write(randomP->fd, msg2, strlen(msg2)) < 0) {
                perror("write");
                exit(1);
            }
        }

        findNewPeer(s);
    }
}
