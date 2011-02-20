
Story
-----

Some of you might remember [Six degrees of Wikipedia](http://www.netsoc.tcd.ie/~mu/wiki/) by Stephen Dolan. That was done in 2007 and I though it would be a good idead to the same several years later and see what might have changed.

I started with a [XML parser](https://github.com/emiraga/wikigraph/blob/f4ee89d28efc93f4b44d7ccea4b036aa3db806f6/xmlparse.py) just like Stephen, but later decided to parse SQL files instead, since they contain all information we need, and admittedly wiki parsing is much more difficult. As far as database goes, I needed one which was in-memory (including VM capabilities), and one that supports more complex data structures such as queues (distributed processing). Needless to say: [**redis**](http://redis.io/) was database of choice.

Dependencies
------------
* [hiredis](https://github.com/antirez/hiredis), located in a directory names `hiredis` from this directory execute
    git clone https://github.com/antirez/hiredis
* [zlib](http://zlib.net/), command `sudo apt-get install zlib1g-dev` in ubuntu.


Mediawiki database schema
-------------------------

Main unit is called `page`, and each `page` can be classified into different namespaces. Two types of namespaces are important here: article and category. Hence each `page` can be of these two types.

Page links can be between any two pages. Meaning that article/category can link any to an article/category. There is another type of link, that is category link. Category link has completely different meaning from page links.


Assumptions
-----------
* pagelinks.sql is exported with sorting `mysqldump --tables pagelinks --order-by-primary`


Performance
-----------

* Replace MD5 with some faster function

