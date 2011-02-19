
#define DUMPFILES "mysqldumps/enwiki-20110115-" //only prefix

/* Using redis hashes is believed to reduce memory usage
 * like they say in this page http://redis.io/topics/memory-optimization */
//#define USE_REDIS_HASH

/* Use 150MB in client program instead of in redis to speed-up computation and
 * reduce memory usage of redis-server */
#define USE_LOCAL_STORAGE

#define REDIS_UNIXSOCKET "/tmp/redis.sock"
//#define REDISHOST "127.0.0.1"
//#define REDISPORT 6379

/* It becomes extremely verbose */
#define DEBUG

#ifdef DEBUG

// Switch to localdump-*.sql files in debug mode
#undef DUMPFILES
#define DUMPFILES "mysqldumps/localdump-"

#endif /* DEBUG */
