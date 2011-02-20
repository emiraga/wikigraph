Story
-----

Some of you might remember [Six degrees of Wikipedia](http://www.netsoc.tcd.ie/~mu/wiki/) by Stephen Dolan. That was done in 2007 and I though it would be a good idea to do same analysis several years later to see what might have changed.

I started with a [XML parser](https://github.com/emiraga/wikigraph/blob/f4ee89d28efc93f4b44d7ccea4b036aa3db806f6/xmlparse.py) just like Stephen, but later decided to parse SQL files instead, since they contain all information we need, and admittedly wiki parsing is much more difficult. As far as database goes, I needed one which was in-memory (including VM capabilities), and one that supports more complex data structures such as queues (distributed processing). Needless to say: [**redis**](http://redis.io/) was database of choice.

Mediawiki database schema
-------------------------

Main unit is called `page`, and each `page` can be classified into different namespaces. Two types of namespaces are important here: article and category. Hence each `page` can be of these two types.

Page links can be between any two pages. Meaning that article/category can link any to an article/category. There is another type of link, that is category link. Category link has completely different meaning from page links.

<img src="http://i.imgur.com/dJlSF.png" alt="" title="Hosted by imgur.com" />

Links shaded in gray are ignored to keep things simple and since they are rarely used on wikipedia.

Try it yourself
---------------

Even without wikipedia dumps you can try how this works on a small mediawiki database.
Run a redis server on a localhost and default port.

	git clone https://emiraga@github.com/emiraga/wikigraph.git
	cd wikigraph
	make hiredis # just once
	make debug
	bin/generate_graph_debug
	bin/process_graph_debug -f 1 #make one background worker
	node analyze.js
	<REDISPATH>redis-cli 

Dependencies
------------
* [hiredis](https://github.com/antirez/hiredis), in a directory `./hiredis`
      make hiredis #run it once
* [zlib](http://zlib.net/), command `sudo apt-get install zlib1g-dev` in ubuntu.


Assumptions
-----------
* 2GB of RAM, since I have 2GB on my computer. Many things in this project would look different if I had more or less RAM on my laptop.
  At most 1.5GB is expected to be used by either redis or some programs from here.
* `pagelinks.sql` is exported with sorting `mysqldump --tables pagelinks --order-by-primary`


