all: server-orig server-deflate server-sendfile server-writev server-splice
CFLAGS=-Wall

server-orig:main.c
	${CC} ${CFLAGS}  -o $@ $< 

server-deflate:main-deflate.c
	${CC} ${CFLAGS} -o $@ $< -lz

server-splice:main-splice.c
	${CC} ${CFLAGS} -D_GNU_SOURCE -o $@ $<  -lz

server-writev:main-writev.c
	${CC} ${CFLAGS} -o $@ $<  -lz


server-sendfile:main-sendfile.c
	${CC} ${CFLAGS} -o $@ $< 

.PHONY:
clean:
	rm server-orig server-deflate server-sendfile server-writev server-splice
