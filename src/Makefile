# Standard release options
#CFLAGS=-static -mcpu=i386 -march=i386 -O2

# High optimization options
#CFLAGS= -mcpu=i686 -march=i686 -O6 -fomit-frame-pointer -fforce-addr \
#  -funroll-loops -frerun-cse-after-loop -frerun-loop-opt -falign-functions=4

# Debugging options
CFLAGS=-g -ggdb -DDEBUG

INCL = -I. -I./telnet `pkg-config --cflags glib-2.0`
LIBS = -lpanel -lcurses -lpcre -lpopt -lm `pkg-config --libs glib-2.0`
CC = gcc -Wall -Wno-unused-but-set-variable -Werror $(INCL)

OBJS = telnet/sock.o telnet/sockbuf.o telnet/telopt.o \
	about.o automap.o autoroam.o character.o client_ai.o combat.o command.o \
	dispatch.o guidebook.o item.o mapview.o menubar.o monster.o mudpro.o \
	navigation.o osd.o parse.o party.o player.o spells.o stats.o timers.o \
	terminal.o utils.o widgets.o

mudpro: $(OBJS)
	gcc -Wall $(CFLAGS) -o mudpro $(INCL) $(OBJS) $(LIBS)

clean::
	for i in $(OBJS) ; do \
		rm -f $$i;\
	done
	rm -f mudpro
