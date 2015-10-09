CC = gcc

LIBS = -lresolv -lsocket -lnsl -lpthread\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\

FLAGS = -g -O2

CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

all: server client

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

# server uses the thread-safe version of readline.c

udpserver.o: udpserver.c
	${CC} ${CFLAGS} -c udpserver.c

server: udpserver.o readline.o get_ifi_info_plus.o
	${CC} ${FLAGS} -o server udpserver.o readline.o get_ifi_info_plus.o ${LIBS}

# client

udpclient.o: udpclient.c
	${CC} ${CFLAGS} -c udpclient.c

client: udpclient.o readline.o get_ifi_info_plus.o
	${CC} ${FLAGS} -o client udpclient.o readline.o get_ifi_info_plus.o ${LIBS}


# pick up the thread-safe version of readline.c from directory "threads"

readline.o: /home/courses/cse533/Stevens/unpv13e_solaris2.10/threads/readline.c
	${CC} ${CFLAGS} -c /home/courses/cse533/Stevens/unpv13e_solaris2.10/threads/readline.c

clean:
	rm server udpserver.o client udpclient.o get_ifi_info_plus.o readline.o

