/* Programs become extremely verbose and use localdumps instead of real ones */
#define DEBUG

#define DUMPFILES "mysqldumps/enwiki-20110115-" //only prefix

/* Use 150MB in client program instead of in redis to speed-up computation and
 * reduce memory usage of redis-server */
#define USE_LOCAL_STORAGE

#define REDIS_UNIXSOCKET "/tmp/redis.sock"
//#define REDISHOST "127.0.0.1"
//#define REDISPORT 6379

#ifdef DEBUG
// Switch to localdump-*.sql files in debug mode
#undef DUMPFILES
#define DUMPFILES "mysqldumps/localdump-"
#endif /* DEBUG */

/* Using redis hashes is supposed to reduce memory usage
 * like they say in this page http://redis.io/topics/memory-optimization 
 * I did not achieve any significand savings, probably because of wrong usage.
 */
//#define USE_REDIS_HASH

