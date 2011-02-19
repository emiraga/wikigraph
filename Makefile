largefile=-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 
cflags=-O3
redisobj=hiredis/hiredis.o hiredis/sds.o hiredis/net.o 
debugflags=-g -ggdb

all: bin/generate_graph

bin/md5.o: md5.c md5.h
	gcc $(cflags) -c md5.c 
	mv md5.o bin/

bin/generate_graph: generate_graph.cpp sqlparser.h config.h bin/md5.o
	g++ $(cflags) $(largefile) generate_graph.cpp bin/md5.o $(redisobj) -lz -o bin/generate_graph

clean:
	rm -f bin/* *.o

