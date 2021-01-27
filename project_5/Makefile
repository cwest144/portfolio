#Chris West, cgw29

CC     = gcc
CFLAGS= -std=c99 -g3 -pedantic -Wall -I/c/cs323/Hwk5/
HWK    = /c/cs323/Hwk5

all: bashLT

bashLT: bashLT.o  ${HWK}/mainBashLT.o ${HWK}/parsley.o
	${CC} ${CFLAGS} -o bashLT bashLT.o ${HWK}/mainBashLT.o ${HWK}/parsley.o