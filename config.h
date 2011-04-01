// Prefix of a location of dumps file
#ifdef DEBUG
#define DUMPFILES "mysqldumps/localdump-"
#else
#define DUMPFILES "mysqldumps/enwiki-20110115-"
#endif

// Redis database configuration
#define REDIS_UNIXSOCKET "/tmp/redis.sock"  // define if possible
#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define REDIS_DATABASE 0  // which database to use 'SELECT db'

// In wiki dumps maximum expected pageid
#define MAX_WIKI_PAGEID 31000000  // current value is 30480288

// While transposing a graph, how much nodes with edges
// can we keep in memory. This is a tradeoff between memory and speed.
#define NODES_PER_PASS (500 * 1000)

