sys = require("sys")
redisnode = require("redis-node")

#used for non-blocking commands
redis = redisnode.createClient()
#used for blocking commands such as blpop
redis_block = redisnode.createClient()

WAIT_SECONDS = 5 # for jobs to complete

job_complete = (job, result)->
    #console.log 'complete',job,result

job_running = (err, result)->
    redis_block.blpop 'queue:running', 0, job_running

    if err is null then setTimeout ->
        [queue, job] = result
        redis.get 'result:' + job, (err, status)->
            if status?
                job_complete job, status
            else
                console.log 'No results for '+job+' reinserting to queue:jobs'
                redis.lpush 'queue:jobs', job
    , WAIT_SECONDS * 1000

job_running('go!')

redis.get "s:count:Graph_nodes", (err, nodes)->
    console.log 'graph_nodes ' + nodes
    nodes = 43 * 1000 * 1000
    node = 1
    bulksize = 100
    granul = 100

    setInterval ->
        redis.llen 'queue:jobs', (err, len)->
            console.log 'queue:jobs len is ', len, ' bulksize', bulksize
            if len < bulksize or len < granul
                bulksize += granul
                for i in [1..bulksize]
                    redis.lpush 'queue:jobs', 'D'+(node+i)
                node += bulksize
            else
                bulksize -= granul if bulksize > 0
    , 1000

