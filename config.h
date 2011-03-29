// Prefix of a location of dumps file
#define DUMPFILES "mysqldumps/localdump-"

// Redis database configuration
#define REDIS_UNIXSOCKET "/tmp/redis.sock"  // define if possible
#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define REDIS_DATABASE 0  // which database to use 'SELECT db'

// How much memory can you spare for generation of graph?
#define MAX_MEM_AVAILABLE (1<<30)  // One GB

// In wiki dumps maximum expected pageid
#define MAX_WIKI_PAGEID 31000000  // current value is 30480288

