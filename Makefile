largefile=-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 
cflags=-O3
redisobj=hiredis/hiredis.o hiredis/sds.o hiredis/net.o 
debugflags=-g -ggdb -pg -DDEBUG

all: bin/generate_graph

# PREREQUISITES
hiredis:
	git clone https://github.com/antirez/hiredis.git
	cd hiredis && make

# RELEASE BUILD

bin/md5.o: md5.c md5.h
	gcc $(cflags) -c md5.c 
	mv md5.o bin/

bin/generate_graph: generate_graph.cpp sqlparser.h config.h bin/md5.o
	g++ $(cflags) $(largefile) generate_graph.cpp bin/md5.o $(redisobj) -lz -o bin/generate_graph

# DEBUG BUILD
debug: bin/generate_graph_debug bin/process_graph_debug

bin/md5_debug.o: md5.c md5.h
	gcc $(cflags) -c md5.c 
	mv md5.o bin/md5_debug.o

bin/generate_graph_debug: generate_graph.cpp sqlparser.h config.h bin/md5_debug.o
	g++ $(cflags) $(largefile) generate_graph.cpp bin/md5_debug.o $(redisobj) -lz -o bin/generate_graph_debug $(debugflags)

bin/process_graph_debug: process_graph.cpp sqlparser.h config.h
	g++ $(cflags) process_graph.cpp $(redisobj) -o bin/process_graph_debug $(debugflags)

clean:
	rm -f bin/* *.o

