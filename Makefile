CC:=g++
WARN:=-Wall -Werror -g
OBJECTS = FileSystem.o

.PHONY: all clean compress compile

all: fs

clean:
	rm -f *.o fs-sim.tar.gz fs

compress:
	tar -cvzf fs-sim.tar.gz Makefile *.cc *.h README.*

compile: FileSystem.cc
	$(CC) $(WARN) -c FileSystem.cc

fs: $(OBJECTS)
	$(CC) $(WARN) -o fs $(OBJECTS)
	echo "\nDone!\n"

FileSystem.o: FileSystem.cc FileSystem.h
