CC = gcc

UNP_DIR = /home/courses/cse533/Stevens/unpv13e_solaris2.10
	
LIBS = -lresolv -lsocket -lnsl -lpthread -lm\
	${UNP_DIR}/libunp.a\

FLAGS = -g -O2

CFLAGS = ${FLAGS} -I${UNP_DIR}/lib

all: server client

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

dgutils.o: dgutils.c
	${CC} ${CFLAGS} -c dgutils.c

# server uses the thread-safe version of readline.c

dgserv.o: dgserv.c
	${CC} ${CFLAGS} -c dgserv.c

udpserver.o: udpserver.c
	${CC} ${CFLAGS} -c udpserver.c

server: udpserver.o get_ifi_info_plus.o dgutils.o dgserv.o
	${CC} ${FLAGS} -o server udpserver.o get_ifi_info_plus.o dgutils.o dgserv.o ${LIBS}

# client

dgcli.o: dgcli.c
	${CC} ${CFLAGS} -c dgcli.c

udpclient.o: udpclient.c
	${CC} ${CFLAGS} -c udpclient.c

dgbuffer.o: dgbuffer.c
	${CC} ${CFLAGS} -c dgbuffer.c

dgcli_impl.o: dgcli_impl.c
	${CC} ${CFLAGS} -c dgcli_impl.c

client: udpclient.o readline.o get_ifi_info_plus.o dgutils.o dgcli.o dgbuffer.o dgcli_impl.o
	${CC} ${FLAGS} -o client udpclient.o readline.o get_ifi_info_plus.o dgutils.o dgcli.o dgbuffer.o dgcli_impl.o ${LIBS}


# pick up the thread-safe version of readline.c from directory "threads"

readline.o: ${UNP_DIR}/threads/readline.c
	${CC} ${CFLAGS} -c ${UNP_DIR}/threads/readline.c

clean:
	rm server udpserver.o client udpclient.o get_ifi_info_plus.o readline.o dgutils.o dgcli.o dgserv.o dgbuffer.o dgcli_impl.o

