sys = require("sys")
redisnode = require("redis-node")

#used for non-blocking commands
redis = redisnode.createClient()
#used for blocking command blpop
redis_block = redisnode.createClient()
#used for pub/sub messages
redis_pubsub = redisnode.createClient()

WAIT_SECONDS = 2 # for jobs to complete
KEEP_CLOSEST = 100 # articles

graph =
    num_nodes : 0
    num_articles : 0
    largest_scc : 0
    articles_done : 0
    reachable_sum : 0
    distance_sum : 0

job_complete = (job, result)->
    return if not job or job[0] != 'D'
    r = JSON.parse(result)
    return if r.error
    node = parseInt job.substr(1), 10
    console.log 'complete', job, ' ', node, "'"+result+"'"
    graph.articles_done += 1
    reachable = 0
    totdistance = 0
    for count, distance in r.count_dist
        # console.log distance, count
        reachable += count
        totdistance += (count * distance)
    graph.distance_sum += totdistance
    graph.reachable_sum += reachable
    if reachable >= graph.largest_scc
        closeness = totdistance / (reachable - 1)
        console.log 'close', closeness
        redis.zadd 's:closest', closeness, node, (err, cnt)->
            console.log 'zadd', err, cnt
            redis.zremrangebyrank 's:closest', KEEP_CLOSEST, -1

    if graph.articles_done is graph.num_articles
        setTimeout -> # wait 1 sec
            console.log 'redis saving...'
            redis.save ->
                redis.close()
                redis_block.close()
                redis_pubsub.close()
                console.log 'Analysis done'
                console.log '-------------'
                print_report()
        , 1000

print_report = ->
    graph.avg_reachable = graph.reachable_sum / graph.num_articles
    graph.avg_distance = graph.distance_sum / graph.reachable_sum
    console.log graph
    console.log 'Average number of reachable nodes:', graph.avg_reachable
    console.log 'Average distance:', graph.avg_distance

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

insert_all_jobs = (command)->
    node = 0
    nodes = graph.num_nodes
    bulksize = 2
    granul = 2

    insert_jobs = ->
        redis.llen 'queue:jobs', (err, len)->
            console.log (100*(node-len)/nodes).toFixed(4) + '% queue:jobs len is ', len, ' bulksize', bulksize
            if len < bulksize or len < granul
                bulksize += granul
                for i in [1..bulksize]
                    redis.lpush 'queue:jobs', command+(node+i)
                node += bulksize
            else
                bulksize -= granul if bulksize > 0

            if node < nodes or len > 0
                setTimeout insert_jobs, 1000
            else
                node = nodes
                console.log 'All jobs are inserted into the queue and queue is empty'
                # might not be completely empty due to re-insertions from queue:running
    insert_jobs('go!')

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

redis.mget "s:count:Graph_nodes", "s:count:Articles", (err, [nodes, articles])->
    console.log 'graph_nodes', nodes
    console.log 'articles', articles
    graph.num_nodes = parseInt nodes, 10
    graph.num_articles = parseInt articles, 10

    # compute SCC of this graph
    graph_get_results 'S', (scc)->
        graph.scc = scc.components
        # Find size of largest component
        for [size, cnt] in scc.components
            size = parseInt size, 10
            console.log 'Total of '+cnt+' component(s) of size: '+size
            graph.largest_scc = size if size > graph.largest_scc
        # Start processing all nodes for computing distances
        insert_all_jobs 'D'

