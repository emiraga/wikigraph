var assert = require('assert');
var redisnode = require("redis-node");
var fs = require("fs");

// {{{ config
var WAIT_SECONDS = 7; // for jobs to complete
var KEEP_CLOSEST = 100; // nodes
var RANDOM_ARTICLES = 0; // Randomly sample X nodes, put 0 for all nodes
var RANDOM_CATEGORIES = 0; // Randomly sample X nodes, put 0 for all nodes
// }}}

// {{{ tmpl - micro-templates for javascript
// Simple JavaScript Templating
// John Resig - http://ejohn.org/ - MIT Licensed
function tmpl(str, data){
  // Generate a reusable function that will serve as a template
  // generator.
  var fn = new Function("obj",
    "var p=[],print=function(){p.push.apply(p,arguments);};" +
    
    // Introduce the data as local variables using with(){}
    "with(obj){p.push('" +
    
    // Convert the template into pure JavaScript
    str.replace(/[\r\t\n]/g, " ")
      .split("<%").join("\t")
      .replace(/((^|%>)[^\t]*)'/g, "$1\r")
      .replace(/\t=(.*?)%>/g, "',$1,'")
      .split("\t").join("');")
      .split("%>").join("p.push('")
      .split("\r").join("\\'") + "');}return p.join('');");
  
  // Provide some basic currying to the user
  return data ? fn( data ) : fn;
};
// }}}

// {{{ class Node
var Node = function(key, value) {
  this.key = key;
  this.value = value;
};
// }}}

// {{{ class Heap
// Based on goog.structs.Heap from Closure Library
// Licensed under the Apache License, Version 2.0
var Heap = function() {
    this.nodes_ = [];
};
Heap.prototype.size = function() {
  return this.nodes_.length;
};
Heap.prototype.insert = function(key, value) {
  var node = new Node(key, value);
  var nodes = this.nodes_;
  nodes.push(node);
  this._moveUp(nodes.length - 1);
};
Heap.prototype._moveUp = function(index) {
  var nodes = this.nodes_;
  var node = nodes[index];
  // While the node being moved up is not at the root.
  while (index > 0) {
    // If the parent is less than the node being moved up, move the parent down.
    var parentIndex = this._getParentIndex(index);
    if (nodes[parentIndex].key > node.key) {
      nodes[index] = nodes[parentIndex];
      index = parentIndex;
    } else {
      break;
    }
  }
  nodes[index] = node;
};
Heap.prototype._getParentIndex = function(index) {
  return Math.floor((index - 1) / 2);
};
Heap.prototype._getLeftChildIndex = function(index) {
  return index * 2 + 1;
};
Heap.prototype._getRightChildIndex = function(index) {
  return index * 2 + 2;
};
Heap.prototype.remove = function() {
  var nodes = this.nodes_;
  var count = nodes.length;
  var rootNode = nodes[0];
  if (count <= 0) {
    return undefined;
  } else if (count == 1) {
    nodes.pop();
  } else {
    nodes[0] = nodes.pop();
    this._moveDown(0);
  }
  return rootNode;
};
Heap.prototype._moveDown = function(index) {
  var nodes = this.nodes_;
  var count = nodes.length;
  // Save the node being moved down.
  var node = nodes[index];
  // While the current node has a child.
  while (index < Math.floor(count / 2)) {
    var leftChildIndex = this._getLeftChildIndex(index);
    var rightChildIndex = this._getRightChildIndex(index);
    // Determine the index of the smaller child.
    var smallerChildIndex = rightChildIndex < count && 
      nodes[rightChildIndex].key < nodes[leftChildIndex].key ? rightChildIndex : leftChildIndex;
    // If the node being moved down is smaller than its children, the node
    // has found the correct index it should be at.
    if (nodes[smallerChildIndex].key > node.key) {
      break;
    }
    // If not, then take the smaller child as the current node.
    nodes[index] = nodes[smallerChildIndex];
    index = smallerChildIndex;
  }
  nodes[index] = node;
};
// }}}

// {{{ test Heap
(function(){
  var h = new Heap();
  assert.strictEqual(undefined, h.remove());
  h.insert(4,"v4");
  h.insert(6,"v6");
  h.insert(5,"v5");
  h.insert(1,"v1");
  assert.strictEqual("v1", h.remove().value);
  assert.strictEqual("v4", h.remove().value);
  assert.strictEqual("v5", h.remove().value);
  assert.strictEqual("v6", h.remove().value);
  assert.strictEqual(undefined, h.remove());
})();
// }}}

// {{{ class GraphInfo
// 
// Usually, there are two instances of GraphInfo:
//  for article links and category links.
//
function GraphInfo(type) {
  this.type = type || '';
  this.num_nodes = 0;
  this.num_links = 0;
  this.largest_scc = 0;  // cut-off point for closeness calculation
  this.nodes_done = 0;
  this.nodes_done_proper = 0; // without error
  this.sample_size = 0;
  this.reachable_sum = 0;
  this.distance_sum = 0;
  this.closest = new Heap();
  this.onevent = {all_done:function(){}};
  this.keep_closest = KEEP_CLOSEST;
  this.dist_spectrum = [];
  this.num_canreachscc = 0;
}
GraphInfo.prototype.on = function(name, cb) {
  this.onevent[name] = cb;
};
GraphInfo.prototype.SetSCC = function(components) {
  this.scc = components;
  for(var i=components.length-1; i>=0; i--) {
    var size = components[i][0];
    var count = components[i][1];
    if(size > this.largest_scc) {
      this.largest_scc = size;
    }
  }
};
GraphInfo.prototype.SetPageRanks = function(ranks) {
  this.pageranks = ranks;
};
GraphInfo.prototype.CalcAvgDistReachable = function(count_dist) {
  var distances = 0;
  var reachable = 0;

  // 'count_dist' at index 'i' holds a count of how many nodes have min-distance
  // equal to 'i'.
  
  // Exclude node itself from reachable count: TODO(user) Is this correct?

  for (var dist = 1, _len = count_dist.length; dist < _len; dist++) {
    var count = count_dist[dist];
    // Accumulate total number of reachable nodes
    reachable += count;
    // Accumulate distances, this will be used to compute average distance.
    distances += count * dist;
  }

  return {
    distances:distances,
    reachable:reachable,
    closeness:distances / Math.max(reachable,1)  // avoid division by zero
  };
};
GraphInfo.prototype.AddInvalidNode = function(node, count_dist) {
  // When there is an error with node, we still need to increment this counter
  this.nodes_done += 1;

  if (this.nodes_done == this.sample_size) {
    this.onevent['all_done']();  // Call an event
  };
};
GraphInfo.prototype.AddNodeDist = function(node, count_dist) {

  // Add to spectrum
  for (var dist = 0, _len = count_dist.length; dist < _len; dist++) {
    var count = count_dist[dist];
    if (this.dist_spectrum[dist]) {
      this.dist_spectrum[dist] += count;
    } else {
      this.dist_spectrum[dist] = count;
    }
  }

  if (this.largest_scc == 0) {
    throw new Error("scc must be computed before adding articles");
  }

  // Accumulate values
  var obj = this.CalcAvgDistReachable(count_dist);
  this.nodes_done += 1;
  this.nodes_done_proper += 1;
  this.distance_sum += obj.distances;
  this.reachable_sum += obj.reachable;

  // Only nodes that can reach largest SCC or more have right to be included
  // in computation of closeness centrality.
  if (obj.reachable + 1 >= this.largest_scc) {  // +1 is for node itself.
    this.closest.insert(-obj.closeness, node);
    if(this.closest.size() > this.keep_closest) {
      this.closest.remove();
    }
    this.num_canreachscc += 1;
  }

  if(!this.sample_size) {
    throw new Error("sample size is not set");
  }

  if (this.nodes_done == this.sample_size) {
    this.onevent['all_done']();  // Call an event
  };
};
// }}}

// {{{ class ReportData
function ReportData() {
  this.art = new GraphInfo('a');
  this.cat = new GraphInfo('c');
  this.interesting_nodes = [];
};
// }}}

// {{{ class JobMonitor
// Make sure that all jobs that are taken by "process_graph" are completed.
function JobMonitor(redis, redis_block) {
  this.redis = redis;
  this.redis_block = redis_block;
  this.wait_milisec = 1000 * WAIT_SECONDS;
  this.waittime_job = {};  // job specific wait time
  this.monitor = false;
  this.onevent = {};
}
JobMonitor.prototype.on = function(name, cb) {
  this.onevent[name].push(cb);
};
JobMonitor.prototype.Start = function(callback) {
  if (this.monitor) {
    throw new Error("JobMonitor is already running");
  }
  var self = this;
  this.monitor = true;
  // Empty queues
  this.redis.del('queue:jobs', 'queue:running', function(err) {
    if (err) throw err;
    self._popqueue_block();
    callback();
  });
};
JobMonitor.prototype.Stop = function() {
  if (!this.monitor) {
    throw new Error("JobMonitor is not started");
  }
  this.monitor = false;
};
JobMonitor.prototype._popqueue_block = function() {
  var self = this;
  this.redis_block.blpop('queue:running', 0, function(){
    self._job_pop.apply(self, arguments);
  });
};
JobMonitor.prototype._job_pop = function(err, result) {
  if (err) throw err;
  var job = 'result:' + result[1];

  var self = this;
  var wait = this.waittime_job[result[1]] || this.wait_milisec;

  setTimeout(function(){
    self.redis.get(job, function(err,status) {
      if (err) throw err;
      if (!status) {
        // Job is lost, push it back into the run-queue
        self.redis.lpush('queue:jobs', result[1]);
        console.log('Re-scheduling job ' + result[1]);
      }
    });
  }, wait); // Jobs are supposed to be finished after a number of seconds

  if (this.monitor) {
    process.nextTick(function() {
      self._popqueue_block.apply(self);  // monitor queue for next job
    });
  } else {
    //this.onevent.stopped();
  }
};
// }}}

// {{{ interface ExtDb
// Interface for external storage
// function ExtDb() {}
// ExtDb.prototype.set(key,val,callback)
// ExtDb.prototype.get(key,callback)
// }}}

// {{{ class Controller
// Controller just adds new jobs to the queue,
// and calls the callback on completion. 
function Controller(redis, redis_pubsub) {
  this.redis = redis;
  this.redis_pubsub = redis_pubsub;
  this.onevent = {};
  this.explore = false;
  this.extdb = null;
}
Controller.prototype.on = function(name, cb) {
  this.onevent[name] = cb;
};
Controller.prototype.setExplore = function(explore) {
  this.explore = explore;
};
Controller.prototype.setExtDb = function(db) {
  this.extdb = db;
}
Controller.prototype._RunJob = function(job, callback) {
  var self = this;
  if (!self.explore) {
    // First listen on a channel where results will be announced
    self.redis_pubsub.subscribeTo('announce:'+job, function(channel, msg) {
      self.redis_pubsub.unsubscribe(channel);
      console.log('Finished Job: '+job);
      callback(job, JSON.parse(msg));
    });
    // Push the job on queue
    self.redis.lpush('queue:jobs', job);
  } else {
    // From explore more, make sure job is added before quiting.
    self.redis.lpush('queue:jobs', job, function(err) {
      if (err) throw err;
      callback(job, {error:'Running is explore mode'});
    });
  }
};
Controller.prototype.RunJob = function(job, callback) {
  // console.log('Running Job: ' + job);
  // Results are cached, check that first
  var self = this;
  if (!this.explore) {
    this.redis.get('result:'+job, function(err, result) {
      if (err) throw err;
      if (result) {
        // console.log('Finished Job (cache): '+job);
        callback(job, JSON.parse(result));
      } else {
        // Result is not in cache, issue a new job
        self._RunJob(job, callback);
      }
    });
  } else {
    this.redis.exists('result:'+job, function(err, doesExist) {
      if (err) throw err;
      if (doesExist) {
        callback(job, {error:'Running is explore mode'});
      } else {
        // Result is not in cache, issue a new job
        self._RunJob(job, callback);
      }
    });
  }
};
/**
 * @param num_nodes   desired number of nodes to be processed
 * @param command     which command to issue, for example 'aD'
 * @param map         OPTIONAL: a generator to provide nodes that need
 *                    processing. (Default: identity function).
 */
Controller.prototype.JobsForNodes = function(num_nodes, command, map, callback) {
  var node = 0;
  // Slow-start
  var bulksize = 50;
  var granul = 50;
  var self = this;
  
  map = map || function(i) { return i; };

  var insert_bulk = function() {
    self.redis.llen('queue:jobs', function(err, len) {
      if (err) throw err;
      var percent = (100*(node-len)/num_nodes).toFixed(4);
      console.log(percent+'%  queue:jobs '+ command +' len is '+len+' bulksize '+bulksize);
      if (len < 3*bulksize || len < 3*granul) {
        if (len < bulksize || len < granul) {
          bulksize += granul;
        }
        var endpoint = Math.min(num_nodes, node + bulksize);
        for(var i=node+1; i<= endpoint; i++) {
          self.RunJob(command + map(i), callback);
        }
        node = endpoint;
      }
      if (node < num_nodes) {
        setTimeout(insert_bulk, 1000);
      } else {
        node = num_nodes;
        console.log('All jobs are inserted into the queue.');
      }
    });
  };
  insert_bulk();
};
/**
 * @param total        number of nodes that need to be resolved.
 * @param cb_get_node  callback for fetching i-th node.
 * @param cb_set_name  callback to writing the info
 *    function(index, in_degree, out_degree, count_dist)
 *    If error is returned from graph_process, cb_set_name will not be called.
 * @param cb_done      callback when all nodes are done.
 */
Controller.prototype.ResolveNames = function(total, cb_get_node, cb_set_name, cb_done) {
  if(!total) {
    cb_done();
  }
  var resolved = 0;
  // Resolve names of articles given list of nodes
  for (var i = 0; i < total; i++) {
    var node = cb_get_node(i);
    this.redis.get("n:" + node, (function(){
      var _i = i; 
      return function(err, name) {
        if (err) throw err;
        if (!name) throw new Error("Invalid article name");

        //Transform to wiki format
        if(name.substr(0,2) == 'a:') {
          name = name.substr(2);
        } else if(name.substr(0,2) == 'c:') {
          name = "Category:"+name.substr(2);
        }
        cb_set_name(_i, name);
        resolved += 1;
        if (resolved == total) {
          cb_done();
        }
      }
    })());
  }
};
/**
 * @param type         either 'a' for article links graph, or 'c' for category links.
 * @param total        number of nodes that need to be resolved.
 * @param cb_get_node  callback for fetching i-th node.
 * @param cb_set_info  callback to writing the info
 *    function(index, in_degree, out_degree, count_dist)
 *    If error is returned from graph_process, cb_set_info will not be called.
 * @param cb_done      callback when all nodes are done.
 */
Controller.prototype.ResolveInfo = function(type, total, cb_get_node, cb_set_info, cb_done) {
  if(!total) {
    cb_done();
  }
  var resolved = 0;
  var self = this;
  // Resolve names of articles given list of nodes
  for (var i = 0; i < total; i++) {
    var node = cb_get_node(i);
    this.RunJob(type+"I"+node, (function(){
      var _i = i; 
      var _node = node;
      return function(job, result) {
        if(result.error) {
          // For categories, error will be reported
          result.in_degree = result.out_degree = 0;
        }

        self.RunJob(type+"D"+_node, function(job, dist) {

          if(dist.error) {
            // For categories in article graph, error will be reported
            // We want this category processed anyway
            dist.count_dist = [];
          }

          cb_set_info(_i, result.in_degree, result.out_degree, dist.count_dist);

          resolved += 1;
          if (resolved == total) {
            cb_done();
          }
        });
      }
    })());
  }
};
// }}}

// {{{ class Mutex
function Mutex(redis) {
  this.redis = redis;
  this.run = false;
  this.period = 1500;  // milisec
}
Mutex.prototype.Start = function(callback) {
  if (this.run)
    throw new Error("Mutex is already running");

  var self = this;
  this.redis.incr('mutex', function(err, cnt1) {
    if (err) throw err;
    // Check if there is another instance running
    setTimeout(function(){
      self.redis.incr('mutex', function(err, cnt2) {
        if (err) throw err;
        if(cnt1 + 1 == cnt2) {
          self.run = true;
          self._incr();
          callback();
        } else {
          throw new Error("Another instance is running");
        }
      });
    }, self.period*2);
  });
};
Mutex.prototype.Stop = function() {
  if (!this.run)
    throw new Error("Mutex was not started");
  this.run = false;
};
Mutex.prototype._incr = function() {
  var self = this;
  setTimeout(function(){
    if(!self.run) return;
    
    self.redis.incr('mutex',function(err, cnt) {
      if (err) throw err;
      self._incr();
    });
  }, this.period);
};
// }}}

// {{{ class Processing
//  Each function that is passed in processing should have prototype:
//    function name(callback) {
//       // when done, callback();
//    }
//  Example of usage:
//    var p = new Processing(
//      [step1, another_step1_in_parallel],
//      [step2],
//      [step3, more_parallel, step3_parallel_task],
//      [finished]
//    );
//    p.run();
function Processing() {
  this.jobs = Array.prototype.slice.call(arguments);
}
Processing.prototype.run = function(i) {
  i = i || 0;
  var jobs = this.jobs;
  if(i >= jobs.length) {
    return;
  }
  var self = this;
  this._parallel(jobs[i], function() {
    self.run(i+1);
  });
}
Processing.prototype._parallel = function(jobs, callback) {
  var total = jobs.length;
  if(!total) {
    callback();
  }
  var processed = 0;
  for(var i = 0; i < total; i++) {
    jobs[i](function() {
      processed += 1;
      if(processed == total) {
        callback();
      }
    });
  }
};
// }}}

// {{{ class GroupPermutation
function GroupPermutation(n, seed) {
  this.n = n;
  this.s = seed || (1 + Math.floor(Math.random()*n));
  while(this._gcd(this.s, this.n) != 1) {
    this.s += 1;
  }
}
GroupPermutation.prototype._gcd = function(a,b) {
  if(b == 0)
    return a;
  return this._gcd(b, a%b);
};
GroupPermutation.prototype.get = function(i) {
  return ((i-1)*this.s)%this.n + 1;
};
// }}}

// {{{ test GroupPermutation
(function(){
  for(var n = 1; n <= 100; n++) {
    var p = new GroupPermutation(n);
    var o = [];
    for(var i=1;i<=n;i++) {
      o.push(p.get(i));
    }
    o = o.sort(function(a,b){return a-b;});
    for(var i=1;i<=n;i++) {
      assert.strictEqual(o[i-1], i);
    }
  }
})();
// }}}

// {{{ class BufferedReader
function BufferedReader(file) {
    this._fd = fs.openSync(file, 'r');
    this._buffer = '';
    this._off = 0;
    this.RSIZE = 16*1024*1024;
    this._file_size = fs.statSync(file).size;
    this._file_read = 0;
}
BufferedReader.prototype._readMore = function (howmuch) {
    var bufnsize = fs.readSync(this._fd, Math.max(howmuch, this.RSIZE), undefined, 'binary');
    this._buffer += bufnsize[0];
    this._file_read += bufnsize[1];
    console.log((this._file_read/this._file_size*100).toFixed(2)+'%');
}
BufferedReader.prototype.peek = function (size) {
    var diff = size - this._buffer.length + this._off;
    if (diff > 0) {
        this._readMore(diff);
    }
    return this._buffer.substr(this._off, size);
}
BufferedReader.prototype.read = function (size) {
    var ret = this.peek(size);
    this._off += size;
    if (this._off > this.RSIZE) {
        // Remove old data from buffer
        this._buffer = this._buffer.substr(this._off);
        this._off = 0;
    }
    return ret;
}
BufferedReader.prototype.eof = function() {
    return this.peek(1).length === 0;
}
// }}}

// {{{ class AofReader
function AofReader(reader) {
    this._r = reader;
    this._on = {}
}
AofReader.prototype.on = function (cmd, cb) {
    this._on[cmd] = cb;
}
AofReader.prototype._readnum = function(chr) {
    var str1 = this._r.peek(15); // For example: $4\r\n
    assert.strictEqual(chr, str1[0]);
    this._r.read(str1.indexOf("\n")+1); // Skip after newline
    return parseInt(str1.substr(1), 10);
}
AofReader.prototype.readSync = function() {
    var r = this._r;
    var on = this._on;
    while(!r.eof()) {
        var numargs = this._readnum('*');
        var args = []
        for(var i=0;i<numargs;i++) {
            var len = this._readnum('$');
            args[i] = r.read(len);
            
            var rn = r.read(2);
            if("\r\n" != rn) {
              console.dir(rn+r.read(100));
              assert.strictEqual("\r\n", r.read(2));
            }
        }
        on[args[0]] && on[args[0]].apply(null, args);
    }
}
// }}}

function main(opts) {
  
  var redis_connect = function() {
    return redisnode.createClient(opts.port, opts.host);
  }

  var redis = redis_connect(); // used for non-blocking commands
  var redis_block = redis_connect(); // used for blocking command blpop
  var redis_pubsub = redis_connect(); // used for pub/sub messages

  var data = new ReportData();
  var mutex = new Mutex(redis);
  var control = new Controller(redis, redis_pubsub);  // pubsub can't be mixed with regular ops
  var monitor = new JobMonitor(redis, redis_block);  // blpop is blocking operation

  var init_mutex = function(callback) {
    mutex.Start(callback);
  };

  var init_monitor = function(callback) {
    monitor.Start(callback);
  };
  
  //var init_mysql = function(callback) {
  //  if(!opts.mysql) return callback();

  //  // opts.mysql = "user:pass@host/database"
  //  var str2 = opts.mysql.split('@',2);
  //  var userpass = str2.split(':',2);
  //  var hostdb = str2.split('/',2);

  //  var mysqlmodule = require("db-mysql");

  //  new mysqlmodule.Database({
  //    hostname: hostdb[0],
  //    user: userpass[0],
  //    password: userpass[1],
  //    database: hostdb[1]
  //  }).on('ready', function() {
  //    control.setExtDb(new MysqlDb(this));
  //    callback();
  //  }).connect();
  //}

  // Get counts
  var init_get_counts = function(callback) {
    redis.mget( "s:count:Graph_nodes", 
        "s:count:Articles", "s:count:Article_links", 
        "s:count:Categories", "s:count:Category_links",
    function(err, result) {
      if (err) throw err;
      data.art.num_nodes = data.cat.num_nodes = parseInt(result[0], 10);

      data.art.num_articles = parseInt(result[1], 10);
      data.art.num_links = parseInt(result[2], 10);
      
      data.cat.num_categories = parseInt(result[3], 10);
      data.cat.num_links = parseInt(result[4], 10);
      callback();
    });
  };

  // Get connected components
  monitor.waittime_job['aS'] = 30*1000;
  monitor.waittime_job['cS'] = 30*1000;

  var init_compute_art_scc = function(callback) {
    control.RunJob('aS', function(job, result) {
      data.art.SetSCC(result.components);
      console.log('Article links SCC complete');
      callback();
    });
  };

  var init_compute_cat_scc = function(callback) {
    control.RunJob('cS', function(job, result) {
      data.cat.SetSCC(result.components);
      console.log('Category links SCC complete');
      callback();
    });
  };

  // Get PageRanks
  monitor.waittime_job['aR'] = 150*1000;
  monitor.waittime_job['cR'] = 150*1000;

  /**
   * @param type        'a' or 'c'
   * @param member_name 'art' or 'cat'
   */
  var gen_compute_pageranks = function(type, member_name) {
    return function(callback) {
      control.RunJob(type+'R', function(job, result) {
        var ranks = result.ranks;
        for(var i = 0; i < ranks.length; i++) {
          data.interesting_nodes.push({node:ranks[i][1]});
        }
        control.ResolveNames(ranks.length,
          function get(i) { return ranks[i][1]; },
          function set(i, name) { ranks[i][2] = name; },
          function done() {
            data[member_name].SetPageRanks(ranks);
            console.log('Page rank complete.');
            callback();
          }
        );
      });
    }
  }

  /**
   * @param type         'a' or 'c'
   * @param member_name  'art' or 'cat'
   * @param sample_size  how many nodes to probe? (0 for all)
   */
  var gen_compute_distances = function(type, member_name, sample_size) {
    return function(callback) {
      var graph_info = data[member_name];
      assert.strictEqual(graph_info.nodes_done, 0);
      graph_info.on('all_done', callback);
      if(!sample_size || sample_size >= graph_info.num_nodes)
        sample_size = graph_info.num_nodes;
      graph_info.sample_size = sample_size;

      var rand = new GroupPermutation(graph_info.num_nodes, opts.seed);
      control.JobsForNodes(sample_size, type+'D',
        function(i) {
          return rand.get(i);
        },
        function(job, result) {
          if (job[0] != type || job[1] != 'D') {
            throw new Error('wrong '+type+'D != '+job);
          }
          var node = parseInt(job.substr(2), 10);
          if (result.error) {
            graph_info.AddInvalidNode(node, result.error);
          } else {
            assert.strictEqual(result.count_dist[0], 1);
            graph_info.AddNodeDist(node, result.count_dist);
          }
        }
      );
    };
  };

  if (opts.aof) {
    // Instead of loading results from redis server, 
    // stream/read them from AOF file.
    gen_compute_distances = function(type, member_name, sample_size) {
      return function(callback) {
        var aof = new AofReader(new BufferedReader(opts.aof));
        var graph_info = data[member_name];
        assert.strictEqual(graph_info.nodes_done, 0);
        graph_info.on('all_done', function(){
          process.nextTick(callback);
        });
        if(!sample_size || sample_size >= graph_info.num_nodes)
          sample_size = graph_info.num_nodes;
        graph_info.sample_size = sample_size;

        var loaded = {};
        var signature = 'result:'+type+'D';
        var aof_on_SET = function(cmd, key, value) {
          if(key.substr(0,signature.length) == signature) {
            var node = parseInt(key.substr(signature.length), 10);
            if (loaded[node]) return;
            loaded[node] = true; // Remove potential duplicates
            var result = JSON.parse(value);
            if (result.error) {
              graph_info.AddInvalidNode(node, result.error);
            } else {
              assert.strictEqual(result.count_dist[0], 1);
              graph_info.AddNodeDist(node, result.count_dist);
            }
          }
        }
        aof.on('SET', aof_on_SET);

        aof.readSync();

        if (graph_info.sample_size != graph_info.nodes_done) {
          console.log('It seems that AOF file is missing some data, run this script with --explore, but first let me try to fix this.');
          for (var node = 1; node <= sample_size; node++) {
            if (!loaded[node]) {
              control.RunJob(type+'D'+node, function(job, result){
                aof_on_SET('SET','result:'+job, JSON.stringify(result));
              });
            }
          }
        } //if data missing
      }; //return function
    };
  }

  var gen_centr_articles = function(type, member_name) {
    return function(callback) {
      var graph_info = data[member_name];
      // Prepare article distance centers for report
      var pos = graph_info.closest.size();
      graph_info.close_centr = [];
      while(pos) {
        var heapnode = graph_info.closest.remove();
        pos -= 1;
        data.interesting_nodes.push({node:heapnode.value});
        graph_info.close_centr[pos] = {
          node:heapnode.value,
          avg_dist: - heapnode.key
        };
      }

      control.ResolveNames(graph_info.close_centr.length,
        function get(i) {
          return graph_info.close_centr[i].node;
        },
        function set(i, name) {
          graph_info.close_centr[i].name = name;
        },
        callback
      );
    };
  };

  var resolve_interesting_names = function(callback) {
    var list = data.interesting_nodes;

    list = list.sort(function (a, b) { return a.node - b.node; });
    // Remove duplicates
    var ret = [list[0]];
    for (var i = 1; i < list.length; i++) {
      if (list[i-1].node !== list[i].node) {
        ret.push(list[i]);
      }
    }
    data.interesting_nodes = ret;

    console.log('Interesing nodes: ' + ret.length);

    control.ResolveNames(data.interesting_nodes.length,
      function get(i) {
        return data.interesting_nodes[i].node;
      },
      function set(i, name) {
        var data_node = data.interesting_nodes[i];
        data_node.name = name;
        data_node.art = {};
        data_node.cat = {};
      },
      function() {
        console.log('Done interesing names.');
        callback();
      }
    );
  };

  var gen_resolve_interesting = function(type, member_name) {
    return function(callback) {
      control.ResolveInfo(type, data.interesting_nodes.length,
        function get(i) {
          return data.interesting_nodes[i].node;
        },
        function set(i, in_degree, out_degree, count_dist) {
          var node_data = data.interesting_nodes[i][member_name];

          node_data.in_degree = in_degree;
          node_data.out_degree = out_degree;
          node_data.count_dist = count_dist;
          node_data.max_dist = Math.max.apply(null, count_dist);

          node_data.stat = data[member_name].CalcAvgDistReachable(count_dist);
        },
        function() {
          console.log('Done interesing info.');
          callback();
        }
      );
    };
  };

  var stop_mutex_monitor = function(callback) {
    mutex.Stop();
    monitor.Stop();
    callback();
  };

  var redis_close = function(callback) {
    redis.close();
    redis_block.close();
    redis_pubsub.close();

    callback();
  };

  var write_report = function(callback) {
    // Write out the report
    fs.readFile('report/index.tpl', 'utf8', function(err, index_tpl) {
      if (err) throw err;
      data.enwiki = "http://en.wikipedia.org/wiki/";
      var cont = tmpl(index_tpl, data);
      fs.writeFile('report/index.html', cont, function (err) {
        if (err) throw err;
        callback();
      });
    });
  };

  var save_state = function(callback) {
    fs.writeFile('report/state.json', JSON.stringify(data), function (err) {
      if (err) throw err;
      callback();
    });
  }

  var load_state = function(callback) {
    fs.readFile('report/state.json', 'utf8', function(err, state) {
      if (err) throw err;
      data = JSON.parse(state);
      callback();
    });
  }

  var proc;

  if (opts.load) {
    // Alternative process, previous data is loaded from state file
    proc = new Processing(
      [load_state],
      [write_report, redis_close]
    );
  }

  if (opts.explore) {
    // Just populate the database with results,
    // Does not generete the report, but it schedules a jobs
    // to be computed.
    console.log("Entering explore mode");

    control.setExplore(true);
    proc = new Processing(
        [init_mutex],
        [init_monitor],
        [init_get_counts],
        //[gen_compute_distances('a','art', RANDOM_ARTICLES)],
        [gen_compute_distances('c','cat', RANDOM_CATEGORIES)],
  
        [redis_close, stop_mutex_monitor],
        [function(){ process.exit(); }] // deadly!
    );
  }

  // Complete description of report generation process
  proc = proc || new Processing(
      [init_mutex],
      [init_monitor],

      [gen_compute_pageranks('a','art'), init_get_counts, init_compute_art_scc, init_compute_cat_scc],

      [gen_compute_distances('a','art', RANDOM_ARTICLES)],
      [gen_compute_distances('c','cat', RANDOM_CATEGORIES)],

      [gen_centr_articles('a','art'), gen_centr_articles('c','cat')],
      
      [resolve_interesting_names],
      [gen_resolve_interesting('a','art')],
      [gen_resolve_interesting('c','cat')],

      [redis_close, stop_mutex_monitor, write_report, save_state],
      [function(){ process.exit(); }] // deadly!
  );
  proc.run();
};

// {{{ tav module - Brain-free command-line options parser for node.js
// https://github.com/akaspin/tav
// https://github.com/akaspin/tav/blob/master/LICENSE
var tav = { };

// Arguments
Object.defineProperty(tav, "args", {
    value : [],
    enumerable : false
});

Object.defineProperty(tav, "set", {
    /**
     * Sets options.
     * @param spec Specification
     * @param banner Banner
     * @param strict Assume unexpected params as error. 
     */
    value : function(spec, banner, strict) {
        spec = spec || {};
        banner = banner || "Usage:";
        var self = this;
        var incoming = process.argv.slice(2); // incoming params

        /**
         * Returns extra items frob b
         * @param a
         * @param b
         * @returns {Array}
         */
        var arrayDiff = function(a, b) {
            return b.filter(function(i) {
                return a.indexOf(i) == -1;
            });
        };
        /**
         * Check conditions. If help setted - always exit.
         * @param parsed Parsed specs
         */
        var check = function(parsed) {
            var end = false, message = "", code = 0, outer = console.log;
            var setted = Object.keys(self);
            var specced = Object.keys(parsed);
            var required = arrayDiff(setted, specced);
            var unexpected = arrayDiff(specced, setted);
            
            // If any required options is not provided - crash it!
            if (required.length) {
                end = true;
                code = 1;
                outer = console.error;
                message += "Required but not provided:\n    --" 
                        + required.join("\n    --") + "\n";
            }
            // + unexpected
            if (unexpected.length) {
                message += "Unexpected options:\n    --"
                        + unexpected.join("\n    --") + "\n";
            }
            message += (message.length ? 
                    "\nRun with --help for more info" : "");
            
            if (strict && message.length) {
                end = true;
                code = 1;
                outer = console.error;
            }
            
            // If --help, exit without error
            if (incoming.indexOf("--help") != -1) {
                end = true;
                code = 0;
                outer = console.log;
                message = Object.keys(parsed).reduce(function(msg, k) {
                    return msg + parsed[k].note + "\n    --" + k
                    + (parsed[k].req ? " *required\n" : "\n");
                    }, "") + "Help. This message.\n    --help\n";
            }
            
            if (end) {
                // exiting
                outer(banner + "\n");
                outer(message);
                process.exit(code);
            }
        };
        
        // Fill spec and default values
        var parsed = {};
        Object.keys(spec).forEach(function(name) {
            var req = spec[name].value === undefined ? true : false;
            var note = spec[name].note || "Note not provided";
            parsed[name] = {
                req : req,
                note : note
            };
            // If value not required - set it
            if (!req) {
                self[name] = spec[name].value;
            }
        });
        
        // Evaluate process.argv
        var numRe = /^[0-9.]+$/;
        incoming.filter(function(chunk) {
            return chunk != "--help" && chunk != "--";
        })
        .forEach(function(chunk) {
            if (chunk.substring(0,2) == "--") {
                var tokens = chunk.substring(2).split("=");
                var name = tokens[0];
                
                // Expected option - evaluate value
                if (tokens.length == 1) {
                    // Boolean
                    var value = true;
                } else {
                    var value = tokens.length > 2 ?
                            tokens.slice(1).join('=') : tokens[1];
                    if (numRe.test(value)) {
                        value = parseFloat(value); 
                    }
                }
                self[name] = value;
            } else {
                // Just argument
                self.args.push(chunk);
            }
        });
        
        check(parsed);
        return this;
    },
    enumerable : false,
    configurable : false
});

// }}}

var opts = tav.set({
  host: {
    note: 'Redis hostname',
    value: 'localhost'
  },
  port: {
    note: 'Redis port',
    value: 6379
  },
  explore: {
    note: 'Populate database with dataset, don\'t retrieve results',
    value: false
  },
  load: {
    note: 'Load data from previous run',
    value: false
  },
  seed: {
    note: 'Seed for randomizer',
    value: 42
  },
  aof: {
    node: 'Load results from redis AOF file',
    value: false
  }
});

main(opts);

