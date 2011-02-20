sys = require("sys")
redisnode = require("redis-node")

#used for non-blocking commands
redis = redisnode.createClient()
#used for blocking command blpop
redis_block = redisnode.createClient()
#used for pub/sub messages
redis_pubsub = redisnode.createClient()

WAIT_SECONDS = 2 # for jobs to complete

graph_get_results = (command, callback)->
    # Results are cached, check that first
    redis.get 'result:'+command, (err, result)->
        if result?
            callback(JSON.parse(result))
        else
            # Result is not in cache, issue a new job
            # First listen on a channel where results will be announced
            redis_pubsub.subscribeTo 'announce:'+command, (channel, msg)->
                callback(JSON.parse(msg))
                redis_pubsub.unsubscribe(channel)
            # Push a job on the queue
            redis.lpush 'queue:jobs', command

job_complete = (job, result)->
    return if not job or job[0] != 'D'
    r = JSON.parse(result)
    return if r.error
    console.log 'complete', job, "'"+result+"'"

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

insert_all_jobs = (nodes)->
    node = 1
    bulksize = 10
    granul = 10
    nodes = 100000

    insert_jobs = ->
        redis.llen 'queue:jobs', (err, len)->
            console.log (100*(node-len)/nodes).toFixed(4) + '% queue:jobs len is ', len, ' bulksize', bulksize
            if len < bulksize or len < granul
                bulksize += granul
                for i in [1..bulksize]
                    redis.lpush 'queue:jobs', 'D'+(node+i)
                node += bulksize
            else
                bulksize -= granul if bulksize > 0

            if node < nodes or len > 0
                setTimeout insert_jobs, 1000
            else
                node = nodes
                console.log 'All jobs are inserted into the queue and queue is empty'
                # might not be completely empty due to re-insertions
    insert_jobs('go!')

redis.get "s:count:Graph_nodes", (err, nodes)->
    console.log 'graph_nodes', nodes
    # compute SCC of this graph
    graph_get_results 'S', (scc)->
        console.log 'scc', scc

redis_pubsub.subscribeTo 'channel1', (channel, msg)->
    console.log 'I got a message from channel1 ', msg
    redis_pubsub.unsubscribe(channel)

redis_pubsub.subscribeTo 'channel2', (channel, msg)->
    console.log 'I got a message from channel2 ', msg
    redis_pubsub.unsubscribe(channel)

redis_pubsub.subscribeTo 'channel3', (channel, msg)->
    console.log 'I got a message from channel3 ', msg
    redis_pubsub.unsubscribe(channel)

