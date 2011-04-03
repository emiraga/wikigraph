Story
-----

Some of you might remember [Six degrees of Wikipedia](http://www.netsoc.tcd.ie/~mu/wiki/) by Stephen Dolan. 
That was done in 2007 and I though it would be a good idea to see what has changed since then.

I started with a [XML parser](https://github.com/emiraga/wikigraph/blob/f4ee89d28efc93f4b44d7ccea4b036aa3db806f6/xmlparse.py) just like Stephen, 
but later decided to parse SQL files instead, since they contain all information we need, and admittedly wiki parsing is much more difficult. 
As far as database goes, I needed one which was in-memory (including VM capabilities), and one that supports more complex data structures such 
as queues (distributed processing). Needless to say: [redis](http://redis.io/) was database of choice.

My code is available for experimentation and finding potential mistakes I might have made.

Mediawiki database schema
-------------------------

Main unit is called `page`, and each page can be classified into different namespaces. Two types of namespaces are important here: **article** and **category**.

Page links can be between any two pages. Meaning that article/category can link to an article or category. 


<img src="http://i.imgur.com/dJlSF.png" alt="" title="Hosted by imgur.com" />

Gray links in above figure are ignored to keep things simple and they are rarely used on wikipedia. Let me know if you have justification for doing otherwise.

Category links have different meaning from page links. Page links are uni-directional, whereas category links are in both directions. 
An article/category **belongs** to another category and some category **has** articles/categories in it.

Try it yourself
---------------

Without wikipedia dumps you can try how this works on a demo mediawiki database.
Run a redis server on a localhost and default port.

	git clone https://emiraga@github.com/emiraga/wikigraph.git
	cd wikigraph
  cmake src/
	make
	bin/generate_graph_debug
	bin/process_graph_debug -f 1 #make one background worker
	coffee analyze.coffee
	<REDISPATH>redis-cli "keys s:*"

To do analysis of real wikipedia database, download dumps from [english wikipedia dumps page](http://dumps.wikimedia.org/enwiki/).  Optionally, you can extract those `*.sql.gz` files.

Edit `config.h` and compile binaries
    make

Start redis server, preferably with unix socket `/tmp/redis.sock`. Generating graph is database-intensive operation, and unix socket will speed things up a bit. 

You can disabled automated background saves, (comment out `save` lines from `redis.conf`) they can be annoying. Command `SAVE` will be issued after graph has been generated.

These are relevant bits from `redis.conf`

    unixsocket /tmp/redis.conf
    #comment out saves
    #save 900 1
    #save 300 10
    #save 60 10000

On a single machine first generate `graph*.bin`
    bin/generate_graph

Analysis can be distributed, each node will need to have a copy of `graph*.bin`. You should start number of workers equal
to the number of cores/processors that node has, for example command for dual core would look like this
    bin/process_graph -r REDISHOST -p PORT -f 2

And finally controller for the whole process
    coffee analyze.coffee

Dependencies
------------
* [redis](http://redis.io/)
* [hiredis](https://github.com/antirez/hiredis), in `./hiredis` directory. To automatically download hiredis and from this directory run
      make hiredis
* [zlib](http://zlib.net/), command `sudo apt-get install zlib1g-dev` in ubuntu.
* [node](https://github.com/ry/node)
* [coffee-script](http://jashkenas.github.com/coffee-script/)
      npm install coffee-script
* [redis-node](https://github.com/bnoguchi/redis-node)
      npm install redis-node

Assumptions
-----------
* Least 1.5GB of RAM, since I have that much on my computer. Many things in this project would look different if I had more or less RAM on my computer.
  At most 1.0GB is expected to be used by either redis or some programs from here.
* Program is compiled and run as 32bit (Maybe 64bit works as well, I did not try it myself).
* `pagelinks.sql` and `categorylinks.sql` are exported with sorting `mysqldump --order-by-primary`


Redis keys
----------

Names of articles and categories are hashed. Hash values are used to 
refer and search for page names and make links.

Keys `a:<hash>` store number **graphId** which is used in
wikigraph to identify nodes (articles and categories).

Keys `a:<hash>` also can hold `w:<wikiId>` which means that name is redirect
and **wikiId** is used in wikipedia database. 

We map **wikiId** to our own **graphId**. 
Relevant: `g_wikigraphId[wikiId] = graphId;`
**wikiId** is only used during generation of graph. In processing stage,
nodes are refered to using **graphId**.

Keys `c:*` are similar as `a:*`. Former is used for categories and latter for
articles.

Keys `w:<wikiId>` keep values `a:<hash>` or `c:<hash>`. This is used for
reverse lookup of hashes based on **wikiId**.

Keys `s:*` store statistical data.


