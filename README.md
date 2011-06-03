Stephen Dolan has done a [Six degrees of Wikipedia](http://www.netsoc.tcd.ie/~mu/wiki/) in 2007. I though it would be a good idea to see what has changed since then, and introduce some new ideas for analysis.

I started with a [XML parser](https://github.com/emiraga/wikigraph/blob/f4ee89d28efc93f4b44d7ccea4b036aa3db806f6/xmlparse.py) just like Stephen, 
but later I have decided to parse SQL files instead, since they contain all information needed, and admittedly wiki parsing is much more difficult. 
As far as database goes, I needed one which was in-memory (including VM capabilities), and one that supports more complex data structures such 
as queues (distributed processing). Needless to say: [redis](http://redis.io/) was database of choice.

Code is available for experimentation and finding potential mistakes.

Mediawiki database schema
-------------------------

Main unit is called `page`, and each page can be classified into different namespaces. Two types of namespaces are important here: **article** and **category**.

Page links can be between any two pages. Meaning that article/category can link to an article or category. 


<img src="http://i.imgur.com/dJlSF.png" alt="" title="Hosted by imgur.com" />

Gray links in above figure are ignored to keep things simple as they are infrequently used in wikipedia. Let me know if you have justification for doing otherwise.

Category links have different meaning from page links. Page links are uni-directional, whereas category links are in both directions. 
An article/category **belongs** to another category and some category **has** articles/categories in it.

Graph algorithms
----------------

Currently there aren't many great algorithms implemented in this package, suggestions are welcome. BFS, non-recursive SCC, pagerank (eigenvector)
power iteration. It also worthy to mention a fast graph reading, writing, transposition, and merging procedures. They are designed to take little memory while
keeping graph operations relatively fast.

Dependencies
------------
* [redis](http://redis.io/)
* [zlib](http://zlib.net/), in ubuntu `sudo apt-get install zlib1g-dev`
* [google-sparsehash](http://code.google.com/p/google-sparsehash/), in ubuntu `sudo apt-get install libsparsehash-dev`
* [node](https://github.com/ry/node)
* [redis-node](https://github.com/bnoguchi/redis-node), `npm install redis-node`

Try it yourself
---------------

Without wikipedia dumps you can try how this works on a demo mediawiki database.
Run a redis server on a localhost and default port.

    git clone https://emiraga@github.com/emiraga/wikigraph.git
    cd wikigraph
    cmake -DCMAKE_BUILD_TYPE=Debug src/
    make
    ./gen_graph
    ./process_graph -f 1 #make one background worker
    node analyze.js

Output is written to `report/index.html`

Real data from enwiki
---------------------

To do an analysis of real wikipedia database, download dumps from [english wikipedia dumps page](http://dumps.wikimedia.org/enwiki/) you only need a couple of files

 - categorylinks.sql.gz
 - category.sql.gz
 - pagelinks.sql.gz
 - page.sql.gz
 - redirect.sql.gz

Prepare redis server: Flush the database. Client timeout is annoying, disable that. Unix sockets are slightly faster that network. Background writes are annoying as well, append only files (AOF) provide a nice alternative. Relevant lines from `redis.conf`

    unixsocket /tmp/redis.sock
    timeout 0
    appendonly yes
    #save 900 1 don't save rdb while processing
    #save 300 10
    #save 60 1000 just comment saves out
    vm-enabled yes
    vm-max-memory 0

On a single machine first generate graphs. Edit settings in `src/config.h.in` and compile binaries in Release mode (with asserts).

    cmake -DCMAKE_BUILD_TYPE=ReleaseAssert src/
    make
    ./gen_graph

Analysis can be distributed, each node will need to have a copy of `artlinks.graph`, `catlinks.graph` and `graph_nodeiscat.bin`. You should start number of workers equal
to the number of cores/processors that node has, for example command for dual core would look like this

    ./process_graph -r REDISHOST -p PORT -f 2

And more verbose is to repeat following command X times.

    ./process_graph -r REDISHOST -p PORT &

And finally start controller for the whole process

    node analyze.js --explore
    node analyze.js --aof=/path/to/redis/appendonly.aof

This will issue jobs, record the results and (second part) write a html report.

If you have 8GB of RAM on the redis server (which i don't) you could turn off the `vm-enabled` in redis, or even you could even not use the AOF.
I don't know what happens exactly, but controller is run this way:

    node analyze.js --explore
    node analyze.js

Explore mode is added to work arround some mysterious and hard-to-reproduce bug with
pubsub in [redis-node](https://github.com/bnoguchi/redis-node).
Speaking of bugs, redis-2.2.7 will fail to restore itself from AOF (some assertion related to `brpoplpush`).

Assumptions
-----------
* At Least 2.0GB of RAM.
* Program is compiled and run on 32bit linux. (I did not try it on 64bit.)
* `pagelinks.sql` and `categorylinks.sql` are exported with sorting `mysqldump --order-by-primary`

