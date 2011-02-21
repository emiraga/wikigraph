#define DUMPFILES "mysqldumps/enwiki-20110115-" //only prefix

#define REDIS_UNIXSOCKET "/tmp/redis.sock" //if possible
#define REDISHOST "127.0.0.1"
#define REDISPORT 6379

#ifdef DEBUG
// Switch to localdump-*.sql files in debug mode
#undef DUMPFILES
#define DUMPFILES "mysqldumps/localdump-"
#endif /* DEBUG */

/* Using redis hashes is supposed to reduce memory usage
 * like they say in this page http://redis.io/topics/memory-optimization 
 * I did not achieve any significand savings, probably because of wrong config.
 */
//#define USE_REDIS_HASH

// Used in debugging, normally 101
#define STOP_AFTER_PRECENT 101

