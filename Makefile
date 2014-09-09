SRCS=prison.cpp
CFLAGS=-std=c++11 -masm=intel -Wall -Wextra
COOKIES=-fstack-protector-all
ASLR=-pie -fPIE
RELRO=-Wl,-z,relro,-z,now
OPT=-O2

LIBS=
OUT=prison
CC=g++

all:
	$(CC) $(CFLAGS) $(COOKIES) $(ASLR) $(RELRO) $(SRCS) -o $(OUT)

debug:
	$(CC) $(CFLAGS) -DDEBUG -g $(SRCS) -o $(OUT)

opt:
	$(CC) $(CFLAGS) $(COOKIES) $(ASLR) $(RELRO) $(SRCS) $(OPT) -o $(OUT)
