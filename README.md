
Story
-----

Some of you might remember [Six degrees of Wikipedia](http://www.netsoc.tcd.ie/~mu/wiki/) by Stephen Dolan. That was done in 2007 and I though I could reboot that idea several years later to see what might have changed.

I started with an XML parser just like Stephen (you can find it in old git commits), but soon decided to parse SQL files instead, since they contain all information we need, and admittedly wiki parsing is much more difficult. As far as database goes, I needed one which was in-memory (including VM capabilities), and one that supports more complex data structures such as queues (distributed processing) and lists. Needless to say: [**redis**](http://redis.io/).


Dependencies
------------
* hiredis (extract to folder named 'hiredis')
* zlib

