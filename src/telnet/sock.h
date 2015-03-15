#ifndef __SOCK_H__
#define __SOCK_H__

/* == telnet options ==================================================== */

/* 0: will   1: wont */
#define OPT_LOCAL_TTYPE         0

/*  0: better non-binary  1: better binary */
/*  2: must non-binary    3: must binary */
#define OPT_LOCAL_BINARY        0
#define OPT_REMOTE_BINARY       0

/*  0: better char mode   1: better line mode */
/*  2: must char mode     3: must line mode */
#define OPT_REMOTE_SGA          2
#define OPT_REMOTE_ECHO         2

#define DEFAULT_PORT	23
#define sockIsAlive()	(sock.alive)

struct
{
	int fd;
	int alive;
} sock;

void sockClose(void);
void sockShutdown(void);
int sockOpen(char *host, char *port);
void sockReadLoop(void);

#endif /* __SOCK_H__ */
