#include <stdio.h>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock.h>
#elif __linux__
#include <sys/socket.h>
#endif

#include "mudpro.h"
#include "sock.h"
#include "sockbuf.h"
#include "terminal.h"


void sockBufRead(void)
{
	int l;

	l = recv(sock.fd, sockBufR.buf, sizeof(sockBufR.buf), 0);
	if(l <= 0)
	{
		sock.alive = 0;

		mudpro_reset_state (TRUE /* disconnected */);
#if 0
		if(l == 0)
			printt ("sock: connection closed by peer");
		else
			printt ("sock: recv failed");
#endif
		return;
	}

	sockBufR.ptr = sockBufR.buf;
	sockBufR.end = sockBufR.buf + l;
}


void sockBufWrite(void)
{
	int wl, l;

	wl = sockBufW.ptr - sockBufW.top;
	if(wl == 0)
		return;

	l = send(sock.fd, sockBufW.top, wl, 0);
	if(l <= 0)
	{
		sock.alive = 0;

		mudpro_reset_state (TRUE /* disconnected */);
#if 0
		if(l == 0)
			printt ("sock: connection closed by peer");
		else
			printt ("sock: send failed");
#endif
		return;
	}
	else if(l < wl)
	{
		sockBufW.top += l;
		return; /* need retry? */
	}

	sockBufW.ptr = sockBufW.top = sockBufW.buf;
	sockBufW.stop = 0;
}


void putSock1(uchar c)
{
	if (!sockIsAlive ())
		return;

	if(sockBufW.ptr >= sockBufW.buf + SOCKBUFW_SIZE)
	{
		if(sockBufW.ptr >= sockBufW.buf + SOCKBUFW_SIZE_A)
		{
			printt ("sockBufW overrun");
			return;
		}
		else
			sockBufW.stop = 1; /* flow control */
	}
	*sockBufW.ptr++ = c;
}


void putSockN(const char *cp, int n)
{
	for(; n > 0; n--,cp++)
		putSock1(*cp);
}
