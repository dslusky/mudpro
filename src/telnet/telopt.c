#include <stdio.h>
#include <stdlib.h>

#include <arpa/telnet.h>

#define TELCMDS
#define TELOPTS

#include "sock.h"
#include "sockbuf.h"
#include "telopt.h"

/* telnet option negotiation module */

static TelOptStates stTabMaster[] = {
/*	[opt]				[local]			[remote] */
	{ TELOPT_BINARY,	{TOR_BETTER},	{TOR_BETTER}	}, /* 0 */
	{ TELOPT_ECHO,		{TOR_MUSTNOT},	{TOR_BETTER}	}, /* 1 */
    { TELOPT_SGA,		{TOR_BETTER},	{TOR_MUST}		}, /* 3 */
    { TELOPT_TTYPE,		{TOR_NEUTRAL},	{TOR_MUSTNOT}	}, /* 24 */
    { -1,				{TOR_MUSTNOT},	{TOR_MUSTNOT}	}  /* default state */
};

TelOptStates *stTab[NTELOPTS]; /* telOptInit() makes it usable */

static /*const*/ TelOptStates *defaultSt; /* used when unknown options come */


/* must call before each telnet session begins */
void telOptReset(void)
{
    TelOptStates *tosp;

    for(tosp = stTabMaster; tosp->opt >= 0; tosp++)
	{
		tosp->local.state =
		tosp->remote.state = 0; /* all options are disabled initially */
		tosp->local.pending =
		tosp->remote.pending = 0;
    }

    telOpt.binsend =
    telOpt.binrecv =
    telOpt.sgasend = 0;
    telOpt.sentReqs = 0;
}


/* must call once before using this module */
void telOptInit(void)
{
    TelOptStates *tosp;
    int i;

    for(tosp = stTabMaster; tosp->opt >= 0; tosp++) ;
    for(i = 0; i < NTELOPTS; i++) stTab[i] = tosp; /* default entry */
    defaultSt = tosp;
    for(tosp-- ; tosp >= stTabMaster; tosp--)
		stTab[tosp->opt] = tosp;
    telOpt.stTab = stTab;
}


static void setReqs(void)
{
    /* binary mode control */
    stTab[TELOPT_BINARY]->local.req = OPT_LOCAL_BINARY;
    stTab[TELOPT_BINARY]->remote.req = OPT_REMOTE_BINARY;
	/* linemode control */
	stTab[TELOPT_SGA]->remote.req = OPT_REMOTE_SGA;
	stTab[TELOPT_ECHO]->remote.req = OPT_REMOTE_ECHO;
	/* terminal-type response control */
	stTab[TELOPT_TTYPE]->local.req = OPT_LOCAL_TTYPE ? TOR_MUSTNOT : TOR_NEUTRAL;
}


/* tell the peer my option-state-to-be requests */
void telOptSendReqs(void)
{
    TelOptStates *tosp;

    setReqs();

    for(tosp = stTabMaster; tosp->opt >= 0; tosp++)
	{
		switch(tosp->local.req)
		{
			case TOR_MUSTNOT:
			case TOR_BETTERNOT:
				if(tosp->local.state == 1)
				{
					putOptCmd(WONT, tosp->opt);
					tosp->local.pending = 1;
				}
				break;
			case TOR_BETTER:
			case TOR_MUST:
				if(tosp->local.state == 0)
				{
					putOptCmd(WILL, tosp->opt);
					tosp->local.pending = 1;
				}
				break;
			default:;
		}
		switch(tosp->remote.req)
		{
			case TOR_MUSTNOT:
			case TOR_BETTERNOT:
				if(tosp->remote.state == 1)
				{
					putOptCmd(DONT, tosp->opt);
					tosp->remote.pending = 1;
				}
				break;
			case TOR_BETTER:
			case TOR_MUST:
				if(tosp->remote.state == 0)
				{
					putOptCmd(DO, tosp->opt);
					tosp->remote.pending = 1;
				}
				break;
			default:;
		}
    }

	telOpt.sentReqs = 1;
}


/* summarize option states into flags */
static void telOptSummarize(void)
{
    telOpt.binsend = stTab[TELOPT_BINARY]->local.state;
    telOpt.binrecv = stTab[TELOPT_BINARY]->remote.state;
    telOpt.sgasend = stTab[TELOPT_SGA]->remote.state;
}


/* telnet option request/response handling */
int telOptHandle(int cmd, int opt)
{
    TelOptState *tostp;
    TelOptStates *tosp;
    int reqState;	/* cmd's requiring state */
    int posiResCmd;	/* positive response command for cmd */
    int negaResCmd;	/* negative response command for cmd */
    TelOptReq mustNegate;	/* must negate if req is this */
    TelOptReq betterNegate;	/* better negate if req is this */
    TelOptReq betterAssert;	/* better assert if req is this */
    TelOptReq mustAssert;	/* must assert if req is this */

    tosp = (opt < NTELOPTS)? stTab[opt] : defaultSt;

    switch(cmd)
	{
		case WILL:
			tostp = &tosp->remote;
			reqState = 1;
			mustNegate = TOR_MUSTNOT;
			betterNegate = TOR_BETTERNOT;
			betterAssert = TOR_BETTER;
			mustAssert = TOR_MUST;
			posiResCmd = DO;
			negaResCmd = DONT;
			break;
		case WONT:
			tostp = &tosp->remote;
			reqState = 0;
			mustNegate = TOR_MUST;
			betterNegate = TOR_BETTER;
			betterAssert = TOR_BETTERNOT;
			mustAssert = TOR_MUSTNOT;
			posiResCmd = DONT;
			negaResCmd = DO;
			break;
		case DO:
			tostp = &tosp->local;
			reqState = 1;
			mustNegate = TOR_MUSTNOT;
			betterNegate = TOR_BETTERNOT;
			betterAssert = TOR_BETTER;
			mustAssert = TOR_MUST;
			posiResCmd = WILL;
			negaResCmd = WONT;
			break;
		case DONT:
			tostp = &tosp->local;
			reqState = 0;
			mustNegate = TOR_MUST;
			betterNegate = TOR_BETTER;
			betterAssert = TOR_BETTERNOT;
			mustAssert = TOR_MUSTNOT;
			posiResCmd = WONT;
			negaResCmd = WILL;
			break;
		default:
			/* fprintf(stderr, "bug\r\n"); */
			exit(1);
	}

    if(tostp->req == mustNegate || tostp->req == betterNegate)
	{
		if(tostp->pending)
		{
			tostp->pending = 0;
			if(tostp->req == mustNegate) /* requirement didn't meet */
				return 1;
			if(tostp->state == !reqState) /* this may not happen */
			{
				tostp->state = reqState;
				putOptCmd(posiResCmd, opt); /* positive response */
			}
		}
		else
			putOptCmd(negaResCmd, opt); /* negative response */
    }
	else /* if (tostp->req == betterAssert or mustAssert or TOR_NEUTRAL) */
	{
		if(tostp->pending)
		{
			tostp->pending = 0;
			/* don't response because cmd is the response of my request */
		}
		else
			putOptCmd(posiResCmd, opt); /* positive response */
		tostp->state = reqState; /* {en,dis}able option as requested */
    }

    telOptSummarize();
    return 0;
}

/* send term-type subnego param */
static void ttypeSBHandle(void)
{
    putSock1(IAC);
    putSock1(SB);
    putSock1(TELOPT_TTYPE);
    putSock1(TELQUAL_IS);
    putSockN("vt100", 5);
    putSock1(IAC);
    putSock1(SE);
}

/* telnet option subnegotiation request handling */
int telOptSBHandle(int opt)
{
    switch(opt)
	{
		case TELOPT_TTYPE:
			ttypeSBHandle();
			break;
		default:
			return 1;
    }

    return 0;
}
