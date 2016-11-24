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

/* dxp.c: the DamExchange driver for the engine */

#include "main.h"

#define COMBUFLEN 128       /* incoming message buffer size (also max. */
                            /* DamExchange message size incl. endcode) */

mev main_event;             /* bits for events that may terminate the search */

char engine_name[33] = "Moby Dam (" __DATE__ ")";
char opponent_name[33];     /* storage for 32 chars + nul */

u32 move_tick;              /* time of last move made */
int playtime_moves;         /* total nr. of moves for game, 0 = unlimited */
int playtime_period;        /* time available for game, in milliseconds */
int timeleft_moves;         /* remaining moves */
int timeleft_period;        /* remaining time for self, in ms */
int timeleft_opponent;      /* remaining time for other player, in ms */
int oppmoves_first;         /* remaining opponent moves adjustment */
u32 move_time;              /* nominal think time for engine */

int test_depth;             /* max iterative search depth */
u32 test_time;              /* max search time per move (ms) */

int our_side;               /* engine's side in the game */
bool game_inprog;           /* game in progress */
int side_moving;            /* side to move in current root position */
int move_number;            /* the current move number */
int game_number;            /* game number in current connection */

SOCKET listen_sock = INVALID_SOCKET;  /* listening socket */
SOCKET conn_sock = INVALID_SOCKET;    /* connection socket */
char tcp_host[256];         /* host name */
char tcp_port[6] = "27531"; /* port number */
char com_buf[COMBUFLEN];    /* incoming message buffer */
char gr_buf[COMBUFLEN];     /* received gamereq msg */
char mv_buf[COMBUFLEN];     /* received move msg */
char ge_buf[COMBUFLEN];     /* received gameend msg */
char br_buf[COMBUFLEN];     /* received backreq msg */
int game_result;            /* from perspective of engine */
char *gameend_text[] =      /* for received gameend */
    {"", "(loss)", "(draw)", "(win)", "?", "?", "?", "?", "?", "?"};

u32 log_reqtick;            /* time of latest gamereq */
FILE *fp_msg;               /* message log file ptr */
char book_file[PATH_MAX] = "book.opn"; /* opening book filename */
char db_dirs[PATH_MAX] = "."; /* directory/ies of database files */
char msg_file[PATH_MAX] = "dxp.log"; /* message log filename */
char out_file[PATH_MAX] = "engine.log"; /* engine log filename */
char pdn_format[PATH_MAX] = "result%d.pdn"; /* pdn log filename format */
bool report_indb;           /* report egdb result to opponent in gameend */
bool report_draw;           /* report regulation draw to opponent in gameend */
bool test_wipe;             /* wipe the tt at the start of each game */
bool test_delay;            /* insert delay between gameacc and first move */
char opt_fen[256];          /* FEN for optimization profiling run */
bool debug_info;            /* print extra debug info */
bool verbose_info;          /* print extra verbose info */
bool do_pondering;          /* search while awaiting opponent move */
bool ponder_state;          /* pondering state for current game */

/* convert ascii number to integer */
/* ptr -> number in input string */
/* width = nr. of digits */
/* returns: integer value */
static int cnv_num(char *ptr, int width)
{
    int  i, x;
    char ch;

    i = 0;
    for (x = 0; x < width; x++)
    {
        ch = ptr[x];
        if (ch < '0' || ch > '9')
        {
            return 0;
        }
        i = 10*i + ch - '0';
    }
    return i;
}

/* convert integer to 2-digit ascii number */
/* ptr -> output string */
/* i = number to convert */
static void add_num(char *ptr, int i)
{
    ptr[0] = i/10%10 + '0';
    ptr[1] = i%10 + '0';
}

/* set nominal move time for the next move */
static void set_movetime(void)
{
    int moves;
    int period;

    if (playtime_moves <= 0)    /* no fixed move limit; */
    {                           /* estimate how many moves remaining */
        moves = 60 + move_number*move_number/196 - move_number;
        moves = max(moves, 15);
    }
    else
    {
        moves = timeleft_moves;
        moves -= moves/4; /* some will be captures that don't take time */
    }
    period = max(timeleft_period, 0); /* never go negative */
    move_time = period/(moves + 2);   /* allow for some search time extension */
}

/* log DamExchange message */
/* buf -> the buffer, nul-terminated */
/* dir -> string indicating direction */
static void log_msg(char *buf, char *dir)
{
    u32 gameticks, nowtick;
    time_t now;

    if (fp_msg != NULL)
    {
        nowtick = get_tick();
        if (buf[0] == 'R') /* set log timestamp relative to gamereq */
        {
            log_reqtick = nowtick;
            now = time(NULL);
            fprintf(fp_msg, "%s", ctime(&now));
        }
        gameticks = nowtick - log_reqtick;
        fprintf(fp_msg, "%010u %05u.%03u %s %s\n",
                nowtick, gameticks/1000, gameticks%1000, dir, buf);
        fflush(fp_msg);
    }
}

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
/* buf -> the buffer, including endcode */
/* len = buffer length, including endcode */
static void tcp_send(char *buf, int len)
{
    if (conn_sock == INVALID_SOCKET || len == 0)
    {
        return;
    }
    if (send(conn_sock, buf, len, 0) != len)
    {
        sockerror("tcp_send");
        closesocket(conn_sock);
        conn_sock = INVALID_SOCKET;
        return;
    }
    log_msg(buf, "snd");
}

/* send DamExchange game accept */
/* accode = acceptance code */
static void send_gameacc(int accode)
{
    char buf[COMBUFLEN];

    buf[0] = 'A';
    memset(&buf[1], ' ', 32);
    strncpy(&buf[1], engine_name, strlen(engine_name)); /* without the nul */
    buf[33] = accode + '0';
    buf[34] = '\0';
    tcp_send(buf, 35);
}

/* send DamExchange move */
/* mvptr -> move structure */
/* time = time used to generate the move (seconds) */
static void send_move(bitboard *mvptr, int time)
{
    char buf[COMBUFLEN];
    u64 capt, captbits;
    int i;

    buf[0] = 'M';
    /* time */
    add_num(&buf[1], time/100);
    add_num(&buf[3], time%100);
    /* from and to */
    add_num(&buf[5], move_square(mvptr, FROM));
    add_num(&buf[7], move_square(mvptr, TO));
    /* count and position of captured pieces */
    captbits = move_captbits(mvptr);
    add_num(&buf[9], popcount(captbits));
    i = 11;
    while (captbits != 0)
    {
        capt = captbits & -captbits;
        captbits -= capt;
        add_num(&buf[i], conv_to_square(capt));
        i += 2;
    }
    buf[i] = '\0';
    tcp_send(buf, i + 1);
}

/* send DamExchange game end */
/* reason = reason to end game */
/* stopcode = stop code */
static void send_gameend(int reason, int stopcode)
{
    char buf[COMBUFLEN];

    buf[0] = 'E';
    buf[1] = reason + '0';
    buf[2] = stopcode + '0';
    buf[3] = '\0';
    tcp_send(buf, 4);
    if (game_result == 0)
    {
        game_result = reason; /* save for log */
    }
}

/* send DamExchange chat string */
/* str -> string to send out */
static void send_chat(char *str)
{
    char buf[COMBUFLEN];

    buf[0] = 'C';
    strncpy(&buf[1], str, COMBUFLEN - 1);
    buf[COMBUFLEN - 1] = '\0';
    tcp_send(buf, strlen(buf) + 1); /* including endcode */
}

/* send DamExchange takeback accept */
/* accept = acceptance code */
static void send_backacc(int accept)
{
    char buf[COMBUFLEN];

    buf[0] = 'K';
    buf[1] = accept + '0';
    buf[2] = '\0';
    tcp_send(buf, 3);
}

/* interpret received console command */
/* buf -> incoming console command, including terminator */
/* len = buf length */
static void rcv_console(char *buf, int len)
{
    debugf("%d bytes console input '%.*s'\n", len, len, buf);

    buf[len - 1] = '\0'; /* bye-bye linefeed */

    if (strcasecmp(buf, "exit") == EQUAL) /* terminate program */
    {
        main_event.cmdexit = TRUE;
    }
    if (strcasecmp(buf, "end") == EQUAL)  /* force sending gameend */
    {
        main_event.cmdend = TRUE;
    }
    if (strncasecmp(buf, "chat ", 5) == EQUAL) /* useless but fun */
    {
        send_chat(&buf[5]);
    }
    if (strcasecmp(buf, "indb") == EQUAL) /* report outcome of a game in a */
    {       /* gameend as soon as an in-database board position is reached */
        report_indb = !report_indb;       /* toggle the setting */
        printf("indb = %s\n", report_indb ? "on" : "off");
        fprintf(stderr, "indb = %s\n", report_indb ? "on" : "off");
    }
    if (strcasecmp(buf, "draw") == EQUAL) /* report regulation draw to */
    {                                     /* opponent in a gameend */
        report_draw = !report_draw;       /* toggle the setting */
        printf("draw = %s\n", report_draw ? "on" : "off");
        fprintf(stderr, "draw = %s\n", report_draw ? "on" : "off");
    }
    if (strcasecmp(buf, "wipe") == EQUAL) /* wipe the tt before each game */
    {                                     /* (for reproducible timing tests) */
        test_wipe = !test_wipe;           /* toggle the setting */
        printf("wipe = %s\n", test_wipe ? "on" : "off");
        fprintf(stderr, "wipe = %s\n", test_wipe ? "on" : "off");
    }
    if (strcasecmp(buf, "delay") == EQUAL) /* insert a delay between */
    {                                      /* gameacc and first move */
        test_delay = !test_delay;          /* toggle the setting */
        printf("delay = %s\n", test_delay ? "on" : "off");
        fprintf(stderr, "delay = %s\n", test_delay ? "on" : "off");
    }
    if (strncasecmp(buf, "depth", 5) == EQUAL) /* search depth limit */
    {
        test_depth = atoi(&buf[5]);
        printf("depth limit = %d\n", test_depth);
        fprintf(stderr, "depth limit = %d\n", test_depth);
    }
    if (strncasecmp(buf, "time", 4) == EQUAL) /* time limit per move */
    {                                         /* (in ms) */
        test_time = atoi(&buf[4]);
        printf("time limit = %u ms\n", test_time);
        fprintf(stderr, "time limit = %u ms\n", test_time);
    }
    if (strcasecmp(buf, "checkend") == EQUAL) /* check endgame files */
    {
        fprintf(stderr, "checking...\n");
        check_enddb();
    }
#ifdef _DEBUG
    if (strcasecmp(buf, "debug") == EQUAL) /* log lots of extra debug info */
    {
        debug_info = !debug_info;          /* toggle the setting */
        printf("debug = %s\n", debug_info ? "on" : "off");
        fprintf(stderr, "debug = %s\n", debug_info ? "on" : "off");
    }
#endif
    if (strcasecmp(buf, "verbose") == EQUAL) /* log extra search info */
    {
        verbose_info = !verbose_info;        /* toggle the setting */
        printf("verbose = %s\n", verbose_info ? "on" : "off");
        fprintf(stderr, "verbose = %s\n", verbose_info ? "on" : "off");
    }
    if (strcasecmp(buf, "?") == EQUAL ||
        strcasecmp(buf, "h") == EQUAL ||
        strcasecmp(buf, "help") == EQUAL)    /* show help */
    {
        fprintf(stderr, "Available console commands are:\n");
        fprintf(stderr, "exit         terminate program\n");
        fprintf(stderr, "end          force sending a gameend\n");
        fprintf(stderr, "chat <text>  send chat text to opponent\n");
        fprintf(stderr, "indb         in-database position ends game (toggle)\n");
        fprintf(stderr, "draw         regulation draw ends game (toggle)\n");
        fprintf(stderr, "wipe         wipe transposition table before each game (toggle)\n");
        fprintf(stderr, "delay        short delay before first move (toggle)\n");
        fprintf(stderr, "depth <n>    set iterative search depth limit (0=no limit)\n");
        fprintf(stderr, "time <n>     set hard time limit per move (in ms) (0=no limit)\n");
        fprintf(stderr, "checkend     check endgame database files\n");
#ifdef _DEBUG
        fprintf(stderr, "debug        log lots of extra debug info (toggle)\n");
#endif
        fprintf(stderr, "verbose      log extra search info (toggle)\n");
        fprintf(stderr, "help         show this help text\n");
    }
}

/* handle received DamExchange message */
/* (can be called from inside the search, which may need to be aborted) */
static void rcv_dxpmsg(void)
{
    char *msg;

    log_msg(com_buf, "rcv");
    switch (com_buf[0])             /* message header */
    {
    case 'R':                       /* gamereq */
        strcpy(gr_buf, com_buf);    /* save for later examination */
        main_event.gamereq = TRUE;
        break;
    case 'A':                       /* gameacc */
        break;                      /* ignore, we don't send gamereq */
    case 'M':                       /* move */
        if (!game_inprog || side_moving == our_side)
        {
            msg = "move received while not expecting one";
            puts(msg);
            send_chat(msg);
            break; /* may continue the search */
        }
        strcpy(mv_buf, com_buf);    /* save for later examination */
        main_event.move = TRUE;
        break;
    case 'E':                       /* gameend */
        strcpy(ge_buf, com_buf);    /* save for later examination */
        main_event.gameend = TRUE;
        break;
    case 'C':                       /* chat */
        fprintf(stderr, "chat> %s\n", &com_buf[1]);
        if (strncasecmp(&com_buf[1], "console ", 8) == EQUAL)
        {                           /* remote console command */
            rcv_console(&com_buf[9], strlen(&com_buf[9]) + 1);
        }
        break;
    case 'B':                       /* backreq */
        strcpy(br_buf, com_buf);    /* save for later examination */
        main_event.backreq = TRUE;
        break;
    case 'K':                       /* backacc */
        break;                      /* ignore, we don't send backreq */
    default:
        printf("unknown dxp msg '%s'\n", com_buf);
        break;
    }
}

/* interpret received tcp stream data */
/* buf -> incoming stream data, may be partial message */
/*        or even multiple messages */
/* len = stream data length */
static void rcv_stream(char *buf, int len)
{
    int i;
    size_t comlen;

    debugf("%d bytes stream input '%.*s'\n", len, len, buf);

    comlen = strlen(com_buf);
    for (i = 0; i < len; i++)
    {
        if (buf[i] != '\0')             /* check for endcode */
        {
            if (comlen < COMBUFLEN - 1)
            {                           /* append to com buffer */
                com_buf[comlen++] = buf[i];
            }
        }
        else
        {                               /* complete message received; */
            com_buf[comlen] = '\0';     /* terminate message */
            rcv_dxpmsg();               /* go interpret it */
            comlen = 0;                 /* reset buffer */
        }
    }
    com_buf[comlen] = '\0';             /* terminate buffer */
}

/* poll for an external event */
/* containing a strange mix of Unix and Windows idioms, */
/* in an effort to maintain portability between the two */
/* wait = max time to block waiting for an event (in milliseconds) */
/* effect: may set main_event to abort search */
void poll_event(int wait)
{
    fd_set rfds;
    struct timeval tv;
    char buf[COMBUFLEN];
    int ret, len;
    SOCKET high, sock;

    FD_ZERO(&rfds);
#ifndef _WIN32
    FD_SET(0, &rfds); /* standard input */
#endif
    high = 0;
    if (listen_sock != INVALID_SOCKET)
    {
        FD_SET(listen_sock, &rfds);
        high = max(high, listen_sock);
    }
    if (conn_sock != INVALID_SOCKET)
    {
        FD_SET(conn_sock, &rfds);
        high = max(high, conn_sock);
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

    if (FD_ISSET(0, &rfds))
    {
        len = read(0, buf, sizeof buf);
        if (len < 0)
        {
            perror("poll_event console input");
        }
        else if (len > 0)
        {
            rcv_console(buf, len);
        }
    }
#else
    /* non-blocking console input for Windows - not pretty, but it will do */
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    while (WaitForSingleObject(h, 0) == WAIT_OBJECT_0)
    {
        static char inbuf[COMBUFLEN];
        size_t inlen;
        char c;
        INPUT_RECORD irec[1];
        DWORD records = 0;

        ret = ReadConsoleInput(h, irec, 1, &records);
        if (ret != 0 && records > 0 &&
            irec[0].EventType == KEY_EVENT &&
            irec[0].Event.KeyEvent.bKeyDown)
        {
            c = irec[0].Event.KeyEvent.uChar.AsciiChar;
            inlen = strlen(inbuf);
            if (c == '\b')             /* backspace */
            {
                if (inlen > 0)
                {                      /* erase last char in buffer */
                    inbuf[inlen - 1] = '\0';
                    fputc(c, stderr);  /* erase last char on screen */
                    fputc(' ', stderr);
                    fputc(c, stderr);
                }
                continue; /* with next input */
            }
            if (c != '\0' && inlen < sizeof inbuf - 1)
            {
                inbuf[inlen++] = c;    /* append char to buffer */
                inbuf[inlen] = '\0';
                fputc(c, stderr);      /* echo char on screen */
            }
            if (c == '\r')             /* enter (return) */
            {
                fputc('\n', stderr);   /* linefeed to screen */
                rcv_console(inbuf, inlen);
                inbuf[0] = '\0';
            }
        }
    }
#endif

    if (listen_sock != INVALID_SOCKET && FD_ISSET(listen_sock, &rfds))
    {
        sock = accept(listen_sock, NULL, NULL);
        if (sock == INVALID_SOCKET)
        {
            sockerror("poll_event accept");
        }
        else
        {
            if (conn_sock != INVALID_SOCKET)
            {
                printf("closing previous connection\n");
                closesocket(conn_sock);
            }
            conn_sock = sock;
            printf("incoming connection accepted\n");
            com_buf[0] = '\0'; /* initialize incoming buffer */
            main_event.connect = TRUE;
        }
    }

    if (conn_sock != INVALID_SOCKET && FD_ISSET(conn_sock, &rfds))
    {
        len = recv(conn_sock, buf, sizeof buf, 0);
        if (len == SOCKET_ERROR)
        {
            sockerror("poll_event recv");
            closesocket(conn_sock);
            conn_sock = INVALID_SOCKET;
        }
        else if (len == 0)
        {
            printf("other end closed connection\n");
            closesocket(conn_sock);
            conn_sock = INVALID_SOCKET;
        }
        else
        {
            rcv_stream(buf, len);
        }
    }
}

/* connect to DamExchange host */
/* returns: TRUE if successful */
static bool tcp_connect(void)
{
    int ret;
    struct addrinfo hints;
    struct addrinfo *servinfo = NULL;
    struct addrinfo *result;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket please */

    ret = getaddrinfo(tcp_host, tcp_port, &hints, &servinfo);
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

        conn_sock = socket(result->ai_family, result->ai_socktype,
                           result->ai_protocol);
        if (conn_sock == INVALID_SOCKET)
        {
            debugf("socket() failed\n");
            continue; /* with next alternative */
        }

        ret = connect(conn_sock, result->ai_addr, result->ai_addrlen);
        if (ret == SOCKET_ERROR)
        {
            debugf("connect() failed\n");
            closesocket(conn_sock);
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
    printf("connected to %s on port %s\n", tcp_host, tcp_port);
    com_buf[0] = '\0'; /* initialize incoming buffer */
    main_event.connect = TRUE;
    return TRUE;
}

/* set up DamExchange listener */
/* returns: TRUE if successful */
static bool tcp_listen(void)
{
    int v6only = 0;
    int reuse = 1;
    int ret;
    struct sockaddr_in6 serv_addr;

    listen_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET)
    {
        sockerror("tcp_listen socket");
        return FALSE;
    }

    /* support both ipv4 and ipv6 connections */
    ret = setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY,
                     (char *) &v6only, sizeof v6only);
    if (ret == SOCKET_ERROR)
    {
        sockerror("tcp_listen setsockopt v6only");
    }

    /* in case previous process recently exited */
    ret = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
                     (char *) &reuse, sizeof reuse);
    if (ret == SOCKET_ERROR)
    {
        sockerror("tcp_listen setsockopt reuse");
    }

    memset(&serv_addr, 0, sizeof serv_addr);
    serv_addr.sin6_family = AF_INET6;
    serv_addr.sin6_addr = in6addr_any;
    serv_addr.sin6_port = htons(atoi(tcp_port));
    ret = bind(listen_sock, (struct sockaddr*)&serv_addr, sizeof serv_addr);
    if (ret == SOCKET_ERROR)
    {
        sockerror("tcp_listen bind");
        return FALSE;
    }

    ret = listen(listen_sock, 0);
    if (ret == SOCKET_ERROR)
    {
        sockerror("tcp_listen listen");
        return FALSE;
    }
    printf("listening on port %s\n", tcp_port);
    return TRUE;
}

/* handle received DamExchange game request */
/* bb -> board structure to fill */
/* returns: TRUE if request is valid */
static bool rcv_gamereq(bitboard *bb)
{
    char *msg;
    int version, x;
    double d = 0.0;

    version = cnv_num(&gr_buf[1], 2);
    if (version != 1)
    {
        msg = "gamereq received with unsupported version";
        puts(msg);
        send_chat(msg);
        send_gameacc(1);
        return FALSE;
    }
    if (game_inprog)
    {
        msg = "gamereq received while already in a game";
        puts(msg);
        send_chat(msg);
        send_gameacc(2);
        return FALSE;
    }
    if (strlen(gr_buf) < 43)
    {
        msg = "gamereq message too short";
        puts(msg);
        send_chat(msg);
        send_gameacc(2);
        return FALSE;
    }
    if (gr_buf[42] == 'A')
    {
        side_moving = W;
        init_board(bb);
    }
    else
    {
        if (strlen(gr_buf) < 94)
        {
            msg = "gamereq message with position too short";
            puts(msg);
            send_chat(msg);
            send_gameacc(2);
            return FALSE;
        }
        side_moving = (gr_buf[43] == 'W') ? W : B;
        empty_board(bb);
        bb->side = side_moving;
        for (x = 1; x <= 50; x++)
        {
            switch (gr_buf[43 + x])
            {
            case 'w':
                place_piece(bb, x, MW);
                break;
            case 'z':
                place_piece(bb, x, MB);
                break;
            case 'W':
                place_piece(bb, x, KW);
                break;
            case 'Z':
                place_piece(bb, x, KB);
                break;
            default:
                break;
            }
        }
    }
    strncpy(opponent_name, &gr_buf[3], 32);
    our_side = (gr_buf[35] == 'W') ? W : B;
    /* read thinking time in minutes (accepting fractions) */
    sscanf(&gr_buf[36], "%3lf", &d);
    playtime_period = (int)(60000.0*d); /* convert to ms */
    if (playtime_period < 1)
    {
        return FALSE;
    }
    playtime_moves = cnv_num(&gr_buf[39], 3);
    return TRUE;
}

/* validate received DamExchange move */
/* listptr -> the list of valid moves */
/* returns: index number of the move in the move list, */
/*          or -1 if the received move is invalid */
static int rcv_move(movelist *listptr)
{
    u64 captbits;
    int from, to, capt, npcapt, i, m;

    if (!game_inprog || side_moving == our_side)
    {
        return -1;
    }

    /* construct move entry */
    from = cnv_num(&mv_buf[5], 2);
    if (from < 1 || from > 50)
    {
        return -1;
    }
    to = cnv_num(&mv_buf[7], 2);
    if (to < 1 || to > 50)
    {
        return -1;
    }
    npcapt = cnv_num(&mv_buf[9], 2);
    if (strlen(mv_buf) < 11 + 2*npcapt)
    {
        return -1;
    }
    captbits = 0;
    for (i = 1; i <= npcapt; i++)
    {
        capt = cnv_num(&mv_buf[9 + 2*i], 2);
        if (capt < 1 || capt > 50)
        {
            return -1;
        }
        captbits |= conv_to_bit(capt);
    }

    /* compare received move with valid moves */
    for (m = 0; m < listptr->count; m++)
    {
        if (from == move_square(&listptr->move[m], FROM) &&
            to == move_square(&listptr->move[m], TO) &&
            captbits == move_captbits(&listptr->move[m]))
        {
            /* move found, is valid */
            return m;
        }
    }
    /* move not found in list */
    return -1;
}

/* handle received DamExchange game end */
/* out: reasonptr -> received reason code */
/* returns: stopcode */
static int rcv_gameend(int *reasonptr)
{
    char *msg;

    if (strlen(ge_buf) < 3)
    {
        msg = "gameend message too short";
        puts(msg);
        send_chat(msg);
        *reasonptr = 0;
        return 0;
    }
    *reasonptr = cnv_num(&ge_buf[1], 1);
    return cnv_num(&ge_buf[2], 1);
}

/* handle received DamExchange takeback request */
/* in/out: bbp = pptr to move chain */
/* returns: TRUE if successful */
static bool rcv_backreq(bitboard **bbp)
{
    bitboard *bb, *parent;
    int movenr, side;
    int n, s;

    if (!game_inprog || strlen(br_buf) < 5)
    {
        return FALSE;
    }
    movenr = cnv_num(&br_buf[1], 3);   /* target move number */
    side = (br_buf[4] == 'W') ? W : B; /* target side to move */
    n = move_number;
    s = side_moving;
    if (movenr > n || (movenr == n && side > s))
    {
        return FALSE;                  /* can't go forward */
    }
    bb = *bbp;
    while (bb != NULL && (n > movenr || s != side))
    {
        if (s == W)
        {
            n--;
        }
        s = W + B - s;
        bb = bb->parent;               /* walk back along the move chain */
    }
    if (bb == NULL)
    {                                  /* walked past the first move? */
        return FALSE;
    }
    while (bb != *bbp)                 /* free the taken-back moves */
    {
        parent = (*bbp)->parent;
        free(*bbp);
        *bbp = parent;
    }
    move_number = n;
    side_moving = s;
    timeleft_moves = playtime_moves - n + 1;  /* adjust remaining moves */
    /* as backreq is intended for correcting operator mistakes, */
    /* the remaining time does not need to be adjusted */
    move_tick = get_tick();                   /* reset time for current move */
    return TRUE;
}

/* free whole board chain of played moves */
/* bb -> current bitboard, representing the most recent move, */
/* with 'parent' reference to previous move in the chain */
static void free_bbchain(bitboard *bb)
{
    bitboard *parent;

    while (bb != NULL)
    {
        parent = bb->parent;
        free(bb);
        bb = parent;
    }
}

/* allocate new bitboard and add to chain of played moves */
/* brd = contents for new bitboard (must have 'parent' set to old bb) */
/* returns: ptr to initialized new bitboard structure */
static bitboard *alloc_bb(bitboard brd)
{
    bitboard *newbb;

    newbb = malloc(sizeof(bitboard));
    if (newbb == NULL)
    {
        printf("alloc_bb: failed to get memory for bitboard\n");
        exit(EXIT_FAILURE);
    }
    *newbb = brd;
    return newbb;
}

/* where it's happening */
static void main_loop(void)
{
    char msg[COMBUFLEN];
    bitboard *bb;
    bitboard brd;
    movelist list;
    lnlist longnotation;
    char nrmoves[12];
    time_t now;
    int m, stopcode, reason, result, lastresult;
    u32 used;
    s32 score;

    lastresult = 0;
    srand(get_tick());
    init_book(book_file);
    init_enddb(db_dirs);
    init_break(db_dirs);

    empty_board(&brd);
    bb = alloc_bb(brd); /* the initial board */

    if (opt_fen[0] != '\0')
    {
        /* do optimization profiling run */
        setup_fen(bb, opt_fen);
        print_board(bb);
        test_time = 10000; /* milliseconds */
        timeleft_moves = playtime_moves = 0;
        timeleft_period = playtime_period = 60*1000*999;
        game_number = move_number = 1;
        side_moving = our_side = bb->side;
        game_inprog = TRUE;
    }

    while (TRUE)
    {
        log_pdnstartstop(bb);

        if (main_event.connect)
        {
            /* connection established */
            game_number = 0;
            main_event.connect = FALSE;
            continue;
        }
        if (main_event.move)
        {
            /* validate received move */
            gen_moves(bb, &list, &longnotation, TRUE); /* generate all moves */
            m = rcv_move(&list);
            if (m < 0)
            {
                sprintf(msg, "invalid move received");
                puts(msg);
                send_chat(msg);
                main_event.move = FALSE;
                continue;
            }

            /* add the move to the chain of played moves */
            bb = alloc_bb(list.move[m]);

            printf("move received from %s\nmove=%d%s ", opponent_name,
                   move_number, (side_moving == W) ? "." : "...");
            print_move_long(&list, m);
            printf("\n");
            side_moving = W + B - side_moving;
            if (side_moving == W)
            {
                move_number++;
            }
            print_board(bb);
            used = get_tick() - move_tick;
            timeleft_opponent -= used;
            strcpy(nrmoves, "unlimited");
            if (playtime_moves > 0)
            {
                sprintf(nrmoves, "%d", timeleft_moves - oppmoves_first);
            }
            printf("opponent used %u ms, leaving %d ms for %s moves\n",
                   used, timeleft_opponent, nrmoves);
            move_tick = get_tick();
            main_event.move = FALSE;
            continue;
        }
        if (main_event.gameend)
        {
            stopcode = rcv_gameend(&reason);
            if (game_inprog)
            {
                printf("gameend received with reason=%d %s\n", reason,
                       gameend_text[reason]);
                if (side_moving == our_side)
                {
                    sprintf(msg, "gameend was unexpected, it's our move");
                    puts(msg);
                    send_chat(msg);
                    if (reason != 0)
                    {
                        reason = 4 - reason; /* convert for side to move */
                    }
                }
                /* assess game outcome */
                result = 0;
                /* check previous and current position for regulation draw */
                if ((bb->parent != NULL && is_draw(bb->parent, 0, NULL)) ||
                    is_draw(bb, 0, NULL))
                {
                    result = 2;
                }
                else if (endgame_value(bb, 0, &score))
                {
                    if (score > INFIN - MAXPLY)
                    {
                        result = 3; /* win */
                    }
                    else if (score < MAXPLY - INFIN)
                    {
                        result = 1; /* loss */
                    }
                    else
                    {
                        result = 2; /* draw */
                    }
                }
                if (result != 0 && reason != 0 && reason != result)
                {
                    printf("gameend reason=%d does not match "
                           "own assessment=%d for side to move\n",
                           reason, result);
                    /* log disputed result as "unknown" */
                    result = 0;
                }
                else if (result == 0 && reason >= 1 && reason <= 3)
                {
                    result = reason; /* accept given gameend reason */
                }
                if (result != 0)
                {
                    if (side_moving != our_side)
                    {
                        result = 4 - result; /* convert to our perspective */
                    }
                    game_result = result;    /* save for log */
                }
                send_gameend(0, stopcode);   /* echo stopcode */
                game_inprog = FALSE;
                log_pdnstartstop(bb);        /* log the current game */
            }
            if (stopcode != 0)
            {
                printf("gameend stopcode=%d, exiting program\n", stopcode);
                fprintf(stderr, "gameend stopcode=%d, exiting program\n", stopcode);
                free_bbchain(bb);   /* release current board chain */
                return;             /* terminate program */
            }
            main_event.gameend = FALSE;
            continue;
        }
        if (main_event.gamereq)
        {
            if (rcv_gamereq(&brd))
            {
                free_bbchain(bb);   /* release last game's board chain */
                bb = alloc_bb(brd); /* initialize board from received gamereq */
                now = time(NULL);
                printf("%sgamereq received from %s\n",
                       ctime(&now), opponent_name);
                strcpy(nrmoves, "unlimited");
                if (playtime_moves > 0)
                {
                    sprintf(nrmoves, "%d", playtime_moves);
                }
                printf("%s moves in %d ms\n",
                       nrmoves, playtime_period);
                print_board(bb);

                /* potentially time-consuming preparations here */
                if (test_wipe)
                {
                    wipe_tt();
                }
                else
                {
                    flush_tt();
                }
                clear_hist();

                send_gameacc(0);

                timeleft_moves = playtime_moves;
                timeleft_period = playtime_period;
                timeleft_opponent = playtime_period;
                move_number = 1;
                move_tick = get_tick();
                game_number++;
                game_result = 0;
                printf("starting game number %d\n", game_number);
                game_inprog = TRUE;
                ponder_state = do_pondering;
                lastresult = 0;
                oppmoves_first = (side_moving == our_side) ? 0 : 1;
                if (test_delay && side_moving == our_side)
                {
                    /* some opponent engines like a bit of delay */
                    /* before we send the first move */
                    usleep(100000);
                }
            }
            main_event.gamereq = FALSE;
            continue;
        }
        if (main_event.backreq)
        {
            if (rcv_backreq(&bb))
            {
                printf("backreq accepted to move %d\n", move_number);
                print_board(bb);
                send_backacc(0);
            }
            else
            {
                sprintf(msg, "invalid backreq message received");
                puts(msg);
                send_chat(msg);
                send_backacc(2);
            }
            main_event.backreq = FALSE;
            continue;
        }
        if (main_event.cmdexit)
        {
            if (game_inprog)
            {
                game_inprog = FALSE;
                log_pdnstartstop(bb);        /* log the current game */
            }
            printf("exiting program\n");
            fprintf(stderr, "exiting program\n");
            free_bbchain(bb);   /* release current board chain */
            return;             /* terminate program */
        }
        if (main_event.cmdend)
        {
            /* to help when dxp protocol is out of sync */
            send_gameend(0, 0);
            game_inprog = FALSE;
            printf("forced gameend sent\n");
            fprintf(stderr, "forced gameend sent\n");
            main_event.cmdend = FALSE;
            continue;
        }

        fflush(stdout);
        fflush(stderr);

        /* pending events have been handled, */
        /* let's see if something new pops up */
        if (game_inprog)
        {
            if (side_moving == our_side)
            {
                poll_event(0);
            }
            else
            {
                if (ponder_state)
                {
                    ponder_state = engine_ponder(bb, 100);
                }
                else
                {
                    poll_event(100);
                }
            }
        }
        else
        {
            poll_event(100);
        }

        if (main_event.any)
        {
            continue; /* in outer while loop to handle new event */
        }

        if (game_inprog && side_moving == our_side)
        {
            /* check previous position for draw */
            if (report_draw && 
                bb->parent != NULL && is_draw(bb->parent, 0, NULL))
            {
                /* we claimed regulation draw after making our last move; */
                /* opponent sent a reply move instead of gameend, so */
                /* now we can send a gameend ourselves */
                send_gameend(2, 0);
                game_inprog = FALSE;
                sprintf(msg, "gameend sent due to claiming regulation draw "
                        "after previous move");
                puts(msg);
                send_chat(msg);
                continue;
            }

            /* check current position for draw */
            result = 0;
            if (is_draw(bb, 0, msg))
            {
                if (lastresult != 0 && lastresult != 2)
                {
                    printf("assessment of game outcome changed from %d to "
                           "regulation draw\n", lastresult);
                    result = 2;
                    lastresult = 2;
                }
                printf("regulation draw = %s\n", msg);
                if (report_draw)
                {
                    send_gameend(2, 0);
                    game_inprog = FALSE;
                    sprintf(msg, "gameend sent due to claiming regulation draw");
                    puts(msg);
                    send_chat(msg);
                    continue;
                }
            }

            gen_moves(bb, &list, &longnotation, TRUE); /* generate all moves */
            if (list.count == 0)
            {
                send_gameend(1, 0);
                game_inprog = FALSE;
                sprintf(msg, "gameend sent due to no valid moves, "
                        "finally admitting defeat");
                puts(msg);
                send_chat(msg);
                continue;
            }

            /* assess game outcome */
            if (result == 0 && endgame_value(bb, 0, &score))
            {
                if (score > INFIN - MAXPLY)
                {
                    result = 3; /* win */
                }
                else if (score < MAXPLY - INFIN)
                {
                    result = 1; /* loss */
                }
                else
                {
                    result = 2; /* draw */
                }
            }
            if (lastresult != 0 && lastresult != result)
            {
                printf("assessment of game outcome changed from %d to %d\n",
                       lastresult, result);
            }
            lastresult = result;

            /* report in-database result? */
            if (report_indb && result != 0)
            {
                send_gameend(result, 0);
                game_inprog = FALSE;
                sprintf(msg, "gameend sent due to in-database reporting, "
                       "reason=%d %s", result, gameend_text[result]);
                puts(msg);
                send_chat(msg);
                continue;
            }

            /* all moves done? */
            if (playtime_moves > 0 && timeleft_moves <= 0)
            {
                send_gameend(result, 0);
                game_inprog = FALSE;
                sprintf(msg, "gameend sent due to all %d moves completed, "
                       "reason=%d %s", playtime_moves, result,
                       gameend_text[result]);
                puts(msg);
                send_chat(msg);
                continue;
            }

            /* let engine determine the next move to be made */
            set_movetime();
            engine_think(&list, 100);
            if (opt_fen[0] != '\0')
            {
                /* terminate optimization profiling run */
                free_bbchain(bb);   /* release current board chain */
                return;
            }
            if (main_event.any) /* search was interrupted? */
            {
                continue; /* in the outer while loop */
            }

            /* add the move to the chain of played moves */
            bb = alloc_bb(list.move[0]);

            printf("move=%d%s ", move_number, (side_moving == W) ? "." : "...");
            print_move_long(&list, 0);
            printf("\n");
            side_moving = W + B - side_moving;
            if (side_moving == W)
            {
                move_number++;
            }
            print_board(bb);
            used = get_tick() - move_tick;
            send_move(bb, ((used + 500)/1000));
            timeleft_moves--;
            timeleft_period -= used;
            strcpy(nrmoves, "unlimited");
            if (playtime_moves > 0)
            {
                sprintf(nrmoves, "%d", timeleft_moves);
            }
            printf("engine used %u ms, leaving %d ms for %s moves\n",
                   used, timeleft_period, nrmoves);
            move_tick = get_tick();

            if (is_draw(bb, 0, msg))
            {
                if (lastresult != 0 && lastresult != 2)
                {
                    printf("assessment of game outcome changed from %d to "
                           "regulation draw\n", lastresult);
                    lastresult = 2;
                }
                printf("regulation draw = %s\n", msg);
                if (report_draw)
                {
                    /* sending a gameend is not allowed right now, */
                    /* announce it but defer the gameend */
                    sprintf(msg, "claiming regulation draw after this move ");
                    sprint_move(msg + strlen(msg), &list.move[0]);
                    puts(msg);
                    send_chat(msg);
                }
            }
        }
    }
}

/* the program entry point */
int main(int argc, char *argv[])
{
    u32 exp = 25;
    time_t now;
    int opt;
    bool ok;
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
        opt = getopt(argc, argv, "b:e:t:zc:p:f:m:l:o:");
        if (opt == -1)
        {
            break; /* done */
        }
        switch (opt)
        {
        case 'b':
            strncpy(book_file, optarg, sizeof book_file - 1);
            break;
        case 'e':
            strncpy(db_dirs, optarg, sizeof db_dirs - 1);
            break;
        case 't':
            exp = atoi(optarg);
            if (exp < 20 || exp > 30)
            {
                printf("exp out of range, using default (25)\n");
                exp = 25;
            }
            break;
        case 'z':
            do_pondering = TRUE;
            break;
        case 'c':
            strncpy(tcp_host, optarg, sizeof tcp_host - 1);
            break;
        case 'p':
            strncpy(tcp_port, optarg, sizeof tcp_port - 1);
            break;
        case 'f':
            strncpy(pdn_format, optarg, sizeof pdn_format - 1);
            break;
        case 'm':
            strncpy(msg_file, optarg, sizeof msg_file - 1);
            break;
        case 'l':
            strncpy(out_file, optarg, sizeof out_file - 1);
            break;
        case 'o':
            strncpy(opt_fen, optarg, sizeof opt_fen - 1);
            break;
        default:
            printf("Usage: %s [-b bookfile] [-e dbdir] [-t exp] [-z] "
                   "[-c host ] [-p port] "
                   "[-f format] [-m msgfile] [-l logfile] "
                   "[-o FEN]\n", argv[0]);
            printf("Engine settings:\n"
                   "  -b bookfile = file name of opening book\n"
                   "       (default: book.opn)\n"
                   "  -e dbdir = directory holding database files\n"
                   "       (or multiple "
#ifdef _WIN32
                                        "semi"
#endif
                                             "colon-separated directories)\n"
                   "       (default: current directory)\n"
                   "  -t exp = exponent of transposition table size, 20..30\n"
                   "       (default: 25 = 2^25 entries = 512MiB)\n"
                   "  -z = do pondering (search while awaiting opponent move)\n");
            printf("DamExchange options:\n"
                   "  -c host = connect to host (dns name or ip address)\n"
                   "       (default: listen instead of connect)\n"
                   "  -p port = port number to use\n"
                   "       (default: 27531)\n");
            printf("Log options:\n"
                   "  -f format = format of pdn log filenames\n"
                   "       %%d in format is replaced by 0, 1, 2 or 3\n"
                   "       where 0=unknown, 1=loss, 2=draw, 3=win for engine\n"
                   "       (default: result%%d.pdn)\n"
                   "  -m msgfile = DamExchange message log filename\n"
                   "       (default: dxp.log)\n"
                   "  -l logfile = engine output log filename\n"
                   "       (default: engine.log)\n");
            printf("Profiling: (used during building of optimized version)\n"
                   "  -o FEN = position to search during 10s, then exit\n");
            exit(EXIT_FAILURE);
        }
    }

    printf("Moby Dam: 10x10 draughts engine with DamExchange protocol\n"
           "Copyright 2015 Harm Jetten\n"
           "This program is free software: you can redistribute it and/or\n"
           "modify it under the terms of the GNU General Public License.\n");

    if (opt_fen[0] != '\0')
    {
        ok = TRUE; /* no tcp connection for optimization profiling run */
    }
    else if (tcp_host[0] != '\0')
    {
        ok = tcp_connect();
    }
    else
    {
        ok = tcp_listen();
    }
    if (ok)
    {
        now = time(NULL);
        fp_msg = fopen(msg_file, "a");
        if (fp_msg == NULL)
        {
            printf("can't open message logfile %s\n", msg_file);
        }
        else
        {
            log_reqtick = get_tick();
            fprintf(fp_msg, "\n%sengine started\n\n", ctime(&now));
            fflush(fp_msg);
        }
        if (freopen(out_file, "a", stdout) == NULL)
        {
            printf("can't open engine logfile %s\n", out_file);
        }
        printf("%s%s\n%s\n", ctime(&now), engine_name,
               "compiled with " TOSTRING(CFLAGS));

        if (!init_tt(exp))
        {
            fprintf(stderr, "tt memory allocation failed\n");
            exit(EXIT_FAILURE);
        }

#ifdef _WIN32
        timeBeginPeriod(1); /* improve resolution of system timer to 1ms */
#endif

        fprintf(stderr, "ready (type h for help)\n");
        main_loop();

#ifdef _WIN32
        timeEndPeriod(1); /* restore default resolution setting */
#endif
        fclose(stdout);
        if (fp_msg != NULL)
        {
            now = time(NULL);
            fprintf(fp_msg, "\n%sengine stopped\n\n", ctime(&now));
            fclose(fp_msg);
        }
        exit(EXIT_SUCCESS);
    }
    return EXIT_FAILURE;
}
