CC=gcc
CFLAGS=-Wall -g
EXE=imgcrypt

all: compile

compile:
	$(CC) $(CFLAGS) -o $(EXE) imgcrypt.c

run: compile
	./$(EXE) -e lenna.bmp data.txt crypt.bmp
	./$(EXE) -d crypt.bmp output.txt
	diff data.txt output.txt
clean:
	rm $(EXE) crypt.bmp output.txt
