CC = gcc

UNP_DIR = /home/courses/cse533/Stevens/unpv13e_solaris2.10

LIBS = -lresolv -lsocket -lnsl -lpthread -lm -lrt\
	${UNP_DIR}/libunp.a\

FLAGS = -g -O2

CFLAGS = ${FLAGS} -I${UNP_DIR}/lib

all: server client

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

dgutils.o: dgutils.c
	${CC} ${CFLAGS} -c dgutils.c

dgserv.o: dgserv.c
	${CC} ${CFLAGS} -c dgserv.c

rtt.o: rtt.c
	${CC} ${CFLAGS} -c rtt.c

rtserv.o: rtserv.c
	${CC} ${CFLAGS} -c rtserv.c

udpserver.o: udpserver.c
	${CC} ${CFLAGS} -c udpserver.c

server: udpserver.o get_ifi_info_plus.o dgutils.o dgserv.o rtserv.o rtt.o
	${CC} ${FLAGS} -o server udpserver.o get_ifi_info_plus.o dgutils.o dgserv.o rtserv.o rtt.o ${LIBS}

# client

dgcli.o: dgcli.c
	${CC} ${CFLAGS} -c dgcli.c

udpclient.o: udpclient.c
	${CC} ${CFLAGS} -c udpclient.c

dgbuffer.o: dgbuffer.c
	${CC} ${CFLAGS} -c dgbuffer.c

dgcli_impl.o: dgcli_impl.c
	${CC} ${CFLAGS} -c dgcli_impl.c

client: udpclient.o get_ifi_info_plus.o dgutils.o dgcli.o dgbuffer.o dgcli_impl.o rtt.o
	${CC} ${FLAGS} -o client udpclient.o get_ifi_info_plus.o dgutils.o dgcli.o dgbuffer.o dgcli_impl.o rtt.o ${LIBS}

clean:
	rm -f server client *.o

