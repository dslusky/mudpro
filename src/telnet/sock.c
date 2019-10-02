#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "character.h"
#include "defs.h"
#include "mudpro.h"
#include "sock.h"
#include "sockbuf.h"
#include "stats.h"
#include "telopt.h"
#include "terminal.h"
#include "timers.h"
#include "utils.h"

static void sockReadLoopState (int c);


void sockClose(void)
{
	if(sock.fd <= 0)
		return;

	if (sockBufRHasData ())
		sockReadLoop ();

	close(sock.fd);
	sock.fd = sock.alive = 0;

	stats.disconnects++;

	timer_stop (timers.idle);
	timer_reset (timers.idle);

	stats.online += (gulong) timer_elapsed (timers.online, NULL);
	timer_stop (timers.online);
	timer_reset (timers.online);

	mudpro_reset_state (TRUE /* disconnected */);
}


void sockShutdown(void)
{
	if(sock.fd <= 0)
		return;

	shutdown(sock.fd, 2);
	sockClose();
}


int sockOpen(char *host, char *port)
{
	struct sockaddr_in sa;
/*	struct in_addr inaddr; */
	struct hostent *he_p;
	int tmp;

	memset(&sa, 0, sizeof(sa));

#if 0 /* XXX this causes problems when DNS not available */
	if(inet_aton(host,&inaddr))
		he_p = gethostbyaddr((char *)&inaddr,sizeof(inaddr),AF_INET);
	else
#endif
		he_p = gethostbyname(host);

	if(he_p == NULL)
	{
		printt ("sock: failed looking up host '%s'", host);
		return 1;
	}

	sa.sin_family = AF_INET;
	if(port != NULL)
		sa.sin_port = htons(atoi(port));
	else
		sa.sin_port = htons(DEFAULT_PORT);

	memcpy(&sa.sin_addr,he_p->h_addr_list[0],sizeof(sa.sin_addr));

	sock.fd = socket(AF_INET, SOCK_STREAM, 0);
	if(sock.fd < 0)
	{
		printt ("sock: socket error");
		return 1;
	}

	tmp = 1;
	if(setsockopt(sock.fd, SOL_SOCKET, SO_OOBINLINE, &tmp, sizeof(tmp)) < 0)
	{
		printt ("sock: setsockopt failed");
		sockClose();
		return 1;
	}

	if(connect(sock.fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
	{
		printt ("sock: connect failed");
		sockShutdown();
		return 1;
	}

	sock.alive = 1;

	return 0;
}

static void sockReadLoopState (int c)
{
	static enum
	{
		SRL_NORM, SRL_IAC, SRL_CMD, SRL_SB,
		SRL_SBC,  SRL_SBS, SRL_SBI
	} state;

	static int cmd;
	static int opt;

	switch (state)
	{
	case SRL_IAC:
		switch (c)
		{
		case WILL:
		case WONT:
		case DO:
		case DONT:
			cmd = c;
			state = SRL_CMD;
			break;
		case IAC:
			terminal_sequence_read (c);
			state = SRL_NORM;
			break;
		case SB:
			state = SRL_SB;
			break;
		default:
			state = SRL_NORM;
		}
		break;
	case SRL_CMD:
		if(telOptHandle (cmd, c)) sock.alive = 0;
		state = SRL_NORM;
		break;
	case SRL_SB:
		opt = c;
		state = SRL_SBC;
		break;
	case SRL_SBC:
		state = (c == TELQUAL_SEND)? SRL_SBS : SRL_NORM;
		break;
	case SRL_SBS:
		state = (c == IAC)? SRL_SBI : SRL_NORM;
		break;
	case SRL_SBI:
		telOptSBHandle (opt);
		state = SRL_NORM;
		break;
	default:
		if (c == IAC) state = SRL_IAC;
		else terminal_sequence_read (c);
	}
}

void sockReadLoop(void)
{
	int c;

	while((c = getSock1()) >= 0)
		sockReadLoopState (c);
}
