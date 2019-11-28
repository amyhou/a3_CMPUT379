CC:=g++
WARN:=-Wall -Werror -g
OBJECTS = FileSystem.o

.PHONY: all clean compress compile

all: fs

clean:
	rm -f *.o fs.tar.gz fs *.txt

compress:
	tar -cvzf fs.tar.gz Makefile *.cc *.h README.*

compile: FileSystem.cc
	$(CC) $(WARN) -c FileSystem.cc

fs: $(OBJECTS)
	$(CC) $(WARN) -o fs $(OBJECTS)
	echo "\nDone!\n"

FileSystem.o: FileSystem.cc FileSystem.h
