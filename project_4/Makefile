#Chris West, cgw29

CC     = gcc
CFLAGS= -std=c99 -g3 -pedantic -Wall -I/c/cs323/Hwk4/
HWK    = /c/cs323/Hwk4

all: lzw encode decode

lzw: lzw.o  ${HWK}/code.o
	${CC} ${CFLAGS} -o lzw lzw.o ${HWK}/code.o

encode: lzw
	ln -f lzw encode

decode: lzw
	ln -f lzw decode