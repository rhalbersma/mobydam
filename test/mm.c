/*
    Copyright 2015 Harm Jetten

    This file is part of Moby Dam.

    Moby Dam is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Moby Dam is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Moby Dam.  If not, see <http://www.gnu.org/licenses/>.
*/

/* mm.c: DamExchange matchmaker */
/* sets up a match between two DamExchange engines running as followers */

#include "test.h"

bool debug_info;               /* print extra debug info */

#define COMBUFLEN 128       /* incoming message buffer size (also max. */
                            /* DamExchange message size incl. endcode) */

#define IDLE    0 /* game states */
#define WAITEND 1
#define INPROG  2

char com_buf[2][COMBUFLEN];    /* incoming message buffer */
char tcp_host[2][256] = { "localhost", "localhost" };
char tcp_port[2][16] = { "27531", "27532" };
SOCKET conn_sock[2] = { INVALID_SOCKET, INVALID_SOCKET };
int game_state[2];
int game_moves = 80;
char game_time[4] = "999";
int game_number;
int result_counts[4];

#ifdef _WIN32
/* print socket error message as text */
/* str -> caller identification */
static void sockerror(char *str)
{
    char msg[256];

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                  FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL,
                  WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR) msg, sizeof msg, NULL);
    fprintf(stderr, "%s: %s\n", str, msg);
}
#endif

/* send out DamExchange message */
/* engine = 0 or 1 */
/* buf -> the buffer, including endcode */
/* len = buffer length, including endcode */
static void tcp_send(int engine, char *buf, int len)
{
    if (conn_sock[engine] == INVALID_SOCKET || len == 0)
    {
        return;
    }
    if (send(conn_sock[engine], buf, len, 0) != len)
    {
        sockerror("tcp_send");
        closesocket(conn_sock[engine]);
        conn_sock[engine] = INVALID_SOCKET;
        return;
    }
}

/* send DamExchange game end */
/* engine = 0 or 1 */
/* reason = reason to end game */
/* stopcode = stop code */
static void send_gameend(int engine, int reason, int stopcode)
{
    char buf[COMBUFLEN];

    buf[0] = 'E';
    buf[1] = reason + '0';
    buf[2] = stopcode + '0';
    buf[3] = '\0';
    tcp_send(engine, buf, 4);
}

/* handle received DamExchange message */
/* engine = 0 or 1 */
/* combuf -> the received message */
static void rcv_dxpmsg(int engine, char *combuf)
{
    int reason;

    switch (combuf[0])              /* message header */
    {
    case 'A':                       /* gameacc */
        if (combuf[33] == '0')
        {
            game_state[engine] = INPROG;
        }
        break;
    case 'M':                       /* move */
        if (game_state[engine] == INPROG &&
            game_state[1 - engine] == INPROG)
        {
            /* pass on to other side */
            tcp_send(1 - engine, combuf, strlen(combuf) + 1);
        }
        break;
    case 'E':                       /* gameend */
        if (game_state[engine] == INPROG)
        {
            /* reply with a gameend, reason=0 */
            send_gameend(engine, 0, 0);
        }
        game_state[engine] = IDLE;

        if (game_state[1 - engine] == INPROG)
        {
            /* pass gameend with reason on to other side */
            reason = combuf[1] - '0';
            send_gameend(1 - engine, reason, 0);
            game_state[1 - engine] = WAITEND;

            if (engine == 1 && reason > 0)
            {
                /* use the perspective of engine 0 */
                reason = 4 - reason;
            }
            result_counts[reason]++;
        }
        break;
    default:
        break;
    }
}

/* interpret received tcp stream data */
/* engine = 0 or 1 */
/* buf -> incoming stream data, may be partial message */
/*        or even multiple messages */
/* len = stream data length */
static void rcv_stream(int engine, char *buf, int len)
{
	int i;
	size_t comlen;
    char *combuf;

    debugf("%d bytes stream input '%.*s'\n", len, len, buf);

    combuf = com_buf[engine];
    comlen = strlen(combuf);
    for (i = 0; i < len; i++)
    {
        if (buf[i] != '\0')             /* check for endcode */
        {
            if (comlen < COMBUFLEN - 1)
            {                           /* append to com buffer */
                combuf[comlen++] = buf[i];
            }
        }
        else                            /* complete message received */
        {
            combuf[comlen] = '\0';      /* terminate message */
            rcv_dxpmsg(engine, combuf);  /* go interpret it */
            comlen = 0;                 /* reset buffer */
        }
    }
    combuf[comlen] = '\0';              /* terminate buffer */
}

/* poll for an external event */
/* wait = max time to block waiting for an event (in milliseconds) */
static void poll_event(int wait)
{
    fd_set rfds;
    struct timeval tv;
    char buf[COMBUFLEN];
    int engine;
    int ret, len;
    SOCKET high;

    FD_ZERO(&rfds);
    high = 0;
    for (engine = 0; engine < 2; engine++)
    {
        if (conn_sock[engine] != INVALID_SOCKET)
        {
            FD_SET(conn_sock[engine], &rfds);
            high = max(high, conn_sock[engine]);
        }
    }
    tv.tv_sec = wait/1000;
    tv.tv_usec = (wait%1000)*1000;
    ret = select((int)high + 1, &rfds, NULL, NULL, &tv);
    if (high != 0 && ret == SOCKET_ERROR)
    {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEINTR)
#else
        if (errno == EINTR)
#endif
        {
            return; /* treat as timeout */
        }
        sockerror("poll_event select");
    }
#ifndef _WIN32
    else if (ret == 0)
    {
        return; /* timeout */
    }
#endif
    for (engine = 0; engine < 2; engine++)
    {
        if (conn_sock[engine] != INVALID_SOCKET && 
            FD_ISSET(conn_sock[engine], &rfds))
        {
            len = recv(conn_sock[engine], buf, sizeof buf, 0);
            if (len == SOCKET_ERROR)
            {
                sockerror("poll_event recv");
                closesocket(conn_sock[engine]);
                conn_sock[engine] = INVALID_SOCKET;
            }
            else if (len == 0)
            {
                printf("%s engine closed connection\n",
                       (engine == 0) ? "first" : "second");
                closesocket(conn_sock[engine]);
                conn_sock[engine] = INVALID_SOCKET;
            }
            else
            {
                rcv_stream(engine, buf, len);
            }
        }
    }
}

/* connect to a DamExchange engine */
/* engine = 0 or 1 */
/* returns: TRUE if successful */
static bool tcp_connect(int engine)
{
    int ret;
    struct addrinfo hints;
    struct addrinfo *servinfo = NULL;
    struct addrinfo *result;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket please */

    ret = getaddrinfo(tcp_host[engine], tcp_port[engine], &hints, &servinfo);
    if (ret != 0)
    {
        fprintf(stderr, "tcp_connect: %s\n", gai_strerror(ret));
        return FALSE;
    }

    for (result = servinfo; result != NULL; result = result->ai_next)
    {
        debugf("trying flags=%d family=%d socktype=%d protocol=%d name=%s\n",
               result->ai_flags, result->ai_family, result->ai_socktype,
               result->ai_protocol, result->ai_canonname);

        conn_sock[engine] = socket(result->ai_family, result->ai_socktype,
                                     result->ai_protocol);
        if (conn_sock[engine] == INVALID_SOCKET)
        {
            debugf("socket() failed\n");
            continue; /* with next alternative */
        }

        ret = connect(conn_sock[engine], result->ai_addr, result->ai_addrlen);
        if (ret == SOCKET_ERROR)
        {
            debugf("connect() failed\n");
            closesocket(conn_sock[engine]);
            continue; /* with next alternative */
        }
        break; /* success */
    }
    freeaddrinfo(servinfo);
    if (result == NULL)
    {
        sockerror("tcp_connect");
        return FALSE;
    }
    return TRUE;
}

/* construct gamereq message */
/* out: msg -> message to construct */
/* bb -> the position to encode in the message */
static void make_gamereq(char *msg, bitboard *bb)
{
    int i, len;
    u64 pcbit;

    len = sprintf(msg, "R01MatchMaker                      %c%3s%03dB%c",
           '?', game_time, game_moves, (bb->side == W) ? 'W' : 'Z');
    for (i = 1; i <= 50; i++)
    {
        pcbit = conv_to_bit(i);
        if (bb->white & pcbit)
        {
            len += sprintf(&msg[len], "%c", (bb->kings & pcbit) ? 'W' : 'w');
        }
        else if (bb->black & pcbit)
        {
            len += sprintf(&msg[len], "%c", (bb->kings & pcbit) ? 'Z' : 'z');
        }
        else
        {
            len += sprintf(&msg[len], "e");
        }
    }
}

/* play one game */
/* msg -> DamExchange gamereq message text */
/* firstmover = engine (0 or 1) to make first move */
static void one_game(char *msg, int firstmover)
{
    /* first set up the side that moves second */
    msg[35] = 'W' + 'Z' - msg[43];
    tcp_send(1 - firstmover, msg, strlen(msg) + 1);
    do {
        /* wait for the gameacc */
        poll_event(1000);
    } while (game_state[1 - firstmover] != INPROG);

    /* then set up the side that moves first */
    msg[35] = msg[43];
    tcp_send(firstmover, msg, strlen(msg) + 1);
    do {
        /* wait for the gameacc */
        poll_event(1000);
    } while (game_state[firstmover] != INPROG);

    game_number++;
    printf("game %d started\n", game_number);
    do {
        /* exchange moves until gameend */
        poll_event(1000);
    } while (game_state[0] != IDLE || game_state[1] != IDLE);
    printf("game %d finished\n", game_number);

    printf("results for first engine: +%d -%d =%d ?%d\n",
           result_counts[3], result_counts[1], 
           result_counts[2], result_counts[0]);
}

/* the program entry point */
int main(int argc, char *argv[])
{
    FILE *fp;
    bitboard brd;
    int opt, port, i;
    bool ok;
    char line[PATH_MAX];
    char msg[COMBUFLEN];
#ifdef _WIN32
    WSADATA wsadata;
    int result;

    result = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (result != 0)
    {
        printf("WSAStartup failed: %d\n", result);
        exit(EXIT_FAILURE);
    }
#endif

    while (TRUE)
    {
        opt = getopt(argc, argv, "m:t:c:p:");
        if (opt == -1)
        {
            break; /* done */
        }
        switch (opt)
        {
        case 'm':
            game_moves = atoi(optarg);
            break;
        case 't':
            strncpy(game_time, optarg, 3);
            while (strlen(game_time) < 3)
            {
                /* prepend zeroes */
                memmove(game_time + 1, game_time, 3);
                game_time[0] = '0';
            }
            break;
        case 'c':
            strcpy(tcp_host[0], optarg);
            break;
        case 'p':
            port = atoi(optarg);
            sprintf(tcp_port[0], "%d", port);
            sprintf(tcp_port[1], "%d", port + 1);
            break;
        default:
            printf("Usage: %s [-m n] [-t n] [-c host] [-p n] matchfile\n", argv[0]);
            printf("  -m n = moves per game (default: 80)\n"
                   "  -t n = time per game (minutes) (default: 999)\n"
                   "         (may be a fraction, e.g. 2.5 or .17, if engines support it)\n"
                   "  -c host = hostname of first engine (default: localhost)\n"
                   "         second engine is always localhost\n"
                   "  -p n = port number of first engine (default: 27531)\n"
                   "         second engine uses port n+1\n"
                   "  matchfile = contains the starting FENs for the match\n");
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc)
    {
        printf("no matchfile arg\n");
        exit(EXIT_FAILURE);
    }
    fp = fopen(argv[optind], "r");
    if (fp == NULL)
    {
        printf("can't open matchfile %s\n", argv[optind]);
        exit(EXIT_FAILURE);
    }

    ok = tcp_connect(0);
    if (!ok)
    {
        printf("failed to connect to %s port %s\n", 
               tcp_host[0], tcp_port[0]);
        exit(EXIT_FAILURE);
    }
    printf("connected to first engine %s port %s\n", 
           tcp_host[0], tcp_port[0]);
    ok = tcp_connect(1);
    if (!ok)
    {
        printf("failed to connect to %s port %s\n", 
               tcp_host[1], tcp_port[1]);
        exit(EXIT_FAILURE);
    }
    printf("connected to second engine %s port %s\n", 
           tcp_host[1], tcp_port[1]);

    /* terminate any leftover game still in progress */
    send_gameend(0, 0, 0);
    send_gameend(1, 0, 0);
    usleep(100000);
    poll_event(0); /* eat any received gameends */

    i = 0;
    while (fgets(line, sizeof line, fp) != NULL)
    {
        i++;
        if (!setup_fen(&brd, line))
        {
            printf("invalid FEN in line %d\n", i);
            exit(EXIT_FAILURE);
        }
        make_gamereq(msg, &brd);

        one_game(msg, 0);

        /* repeat the game with the other side to move first */

        one_game(msg, 1);
    }

    fclose(fp);
    closesocket(conn_sock[0]);
    closesocket(conn_sock[1]);

    printf("called with arguments: ");
    for (i = 1; i < argc; i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\n");
    return EXIT_SUCCESS;
}
