Story
-----

Some of you might remember [Six degrees of Wikipedia](http://www.netsoc.tcd.ie/~mu/wiki/) by Stephen Dolan. That was done in 2007 and I though it would be a good idea to do same analysis several years later to see what might have changed.

I started with a [XML parser](https://github.com/emiraga/wikigraph/blob/f4ee89d28efc93f4b44d7ccea4b036aa3db806f6/xmlparse.py) just like Stephen, but later decided to parse SQL files instead, since they contain all information we need, and admittedly wiki parsing is much more difficult. As far as database goes, I needed one which was in-memory (including VM capabilities), and one that supports more complex data structures such as queues (distributed processing). Needless to say: [redis](http://redis.io/) was database of choice.

Mediawiki database schema
-------------------------

Main unit is called `page`, and each `page` can be classified into different namespaces. Two types of namespaces are important here: *article* and *category*. Hence each `page` can be of these two types.

Page links can be between any two pages. Meaning that article/category can link any to an article/category. Category link has different meaning from page links.

<img src="http://i.imgur.com/dJlSF.png" alt="" title="Hosted by imgur.com" />

Gray links in above figure are ignored to keep things simple and since they are rarely used on wikipedia. Let me know if you have justification for doing otherwise.

Page links are uni-directional whereas category links are in both directions. An article/category *belongs* to a category and a category *has* articles/categories in it.

Try it yourself
---------------

Without wikipedia dumps you can try how this works on a demo mediawiki database.
Run a redis server on a localhost and default port.

	git clone https://emiraga@github.com/emiraga/wikigraph.git
	cd wikigraph
	make hiredis # just once
	make debug
	bin/generate_graph_debug
	bin/process_graph_debug -f 1 #make one background worker
	coffee analyze.coffee
	<REDISPATH>redis-cli "keys s:*"

To do an analysis of a real wikipedia database, download dumps from [wikipedia dumps page](http://dumps.wikimedia.org/enwiki/).  Optionally, you can extract those `*sql..gz` files.

Edit `config.h` and compile binaries
    make
Start redis server, preferably with unix socket `/tmp/redis.sock`.

On a single machine first generate `graph*.bin`
    bin/generate_graph

Analysis can be distributed, each node will need to have a copy of `graph*.bin`. You should start number of workers related to number of cores/processors that node has, for example dual core would look like this
    bin/process_graph -r REDISHOST -p PORT -f 2

And finally controller for the whole process
    coffee analyze.coffee

Dependencies
------------
* [redis](http://redis.io/)
* [hiredis](https://github.com/antirez/hiredis), in `./hiredis`. To do this automatically from this directory run
      make hiredis
* [zlib](http://zlib.net/), command `sudo apt-get install zlib1g-dev` in ubuntu.
* [node](https://github.com/ry/node)
* [coffee-script](http://jashkenas.github.com/coffee-script/)
      npm install coffee-script
* [redis-node](https://github.com/bnoguchi/redis-node)
      npm install redis-node

Assumptions
-----------
* Least 2GB of RAM, since I have 2GB on my computer. Many things in this project would look different if I had more or less RAM on my laptop.
  At most 1.5GB is expected to be used by either redis or some programs from here.
* `pagelinks.sql` is exported with sorting `mysqldump --tables pagelinks --order-by-primary`


