var sys = require("sys");
var redisnode = require("redis-node");

//used for non-blocking commands
var redis = redisnode.createClient();
//used for blocking command blpop
var redis_block = redisnode.createClient();
//used for pub/sub messages
var redis_pubsub = redisnode.createClient();

var WAIT_SECONDS = 2; // for jobs to complete
var KEEP_CLOSEST = 100; // articles

var graph = {
    num_nodes : 0,
    num_articles : 0,
    largest_scc : 0,
    articles_done : 0,
    reachable_sum : 0,
    distance_sum : 0
};

// computing distances from nodes
var job_D_complete = function(job, result) {
  if(!job || job[0] != 'D')
    return;
  r = JSON.parse(result);
  if(r.error)
    return;
  var node = parseInt(job.substr(1), 10);
  console.log('complete', job, ' ', node, "'"+result+"'");

  graph.articles_done += 1;
  var reachable = 0;
  var totdistance = 0;

  var cdist = r.count_dist;
  for (var distance = 0, _len = cdist.length; distance < _len; distance++) {
    count = cdist[distance];
    reachable += count;
    totdistance += count * distance;
  }
  graph.distance_sum += totdistance
  graph.reachable_sum += reachable

  if(reachable >= graph.largest_scc) {
    var closeness = totdistance / (reachable - 1);
    console.log('close', closeness)
    redis.zadd('s:closest', closeness, node, function(err, cnt) {
      console.log('zadd', err, cnt);
      redis.zremrangebyrank('s:closest', KEEP_CLOSEST, -1);
    }
  }

  if(graph.articles_done == graph.num_articles) {
    setTimeout(function() { // # wait 1 sec, why?
      console.log('redis saving...');
      redis.save(function(){
        redis.close();
        redis_block.close();
        redis_pubsub.close();
        console.log('Analysis done');
        console.log('-------------');
        print_report();
      });
    }, 1000);
  }
};

var print_report = function() {
  graph.avg_reachable = graph.reachable_sum / graph.num_articles;
  graph.avg_distance = graph.distance_sum / graph.reachable_sum;
  console.log(graph);
  console.log('Average number of reachable nodes:', graph.avg_reachable);
  console.log('Average distance:', graph.avg_distance);
}

var monitor_running_jobs = function(err, result) {
  redis_block.blpop('queue:running', 0, monitor_running_jobs);

  if(err == null) {
    setTimeout(function() {
      var queue = result[0];
      var job = result[1];
      redis.get('result:' + job, function(err, status) {
        if(status) {
          job_complete(job, status);
        } else {
          console.log('No results for '+job+' reinserting to queue:jobs');
          redis.lpush('queue:jobs', job);
        }
      });
    }, WAIT_SECONDS * 1000);
  }
};


var insert_all_jobs = function(command) {
  var node = 0
  var nodes = graph.num_nodes
  var bulksize = 2
  var granul = 2

  var insert_jobs = function() {
    redis.llen('queue:jobs', function(err, len) {
      console.log( (100*(node-len)/nodes).toFixed(4) + 
        '% queue:jobs len is ', len, ' bulksize', bulksize);
      if(len < bulksize || len < granul) {
        bulksize += granul
        for(var i=1; i<= bulksizel; i++) {
          redis.lpush('queue:jobs', command+(node+i));
        }
        node += bulksize
      } else {
        if(bulksize > 0) {
          bulksize -= granul;
        }
      }
      if(node < nodes or len > 0) {
        setTimeout(insert_jobs, 1000);
      } else {
        node = nodes;
        console.log('All jobs are inserted into the queue and queue is empty');
        // might not be completely empty due to re-insertions from queue:running
      }
    }
  };
  insert_jobs('go!')
};

var get_results = function(command, callback) {
  // Results are cached, check that first
  redis.get('result:'+command, function(err, result) {
    if(result && !err) {
      callback(JSON.parse(result))
    } else {
      // Result is not in cache, issue a new job
      // First listen on a channel where results will be announced
      redis_pubsub.subscribeTo('announce:'+command, function(channel, msg) {
        callback(JSON.parse(msg));
        redis_pubsub.unsubscribe(channel);
      });
      // Push a job on the queue
      redis.lpush('queue:jobs', command);
    }
};

monitor_running_jobs('go!');

redis.mget("s:count:Graph_nodes", "s:count:Articles", function(err, result) {
  var nodes = result[0];
  var articles = result[1];
  console.log('graph_nodes', nodes);
  console.log('articles', articles);
  graph.num_nodes = parseInt(nodes, 10);
  graph.num_articles = parseInt(articles, 10);

  // compute SCC of this graph
  graph_get_results 'S', (scc)->
    graph.scc = scc.components
    // Find size of largest component
    for [size, cnt] in scc.components
      size = parseInt size, 10
      console.log 'Total of '+cnt+' component(s) of size: '+size
      graph.largest_scc = size if size > graph.largest_scc
    // Start processing all nodes for computing distances
    insert_all_jobs 'D'
};

