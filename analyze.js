var assert = require('assert');
var redisnode = require("redis-node");
var fs = require("fs");

// {{{ config
var WAIT_SECONDS = 1; // for jobs to complete
var KEEP_CLOSEST = 100; // articles
var RANDOM_ARTICLES = 100; // Randomly sample X articles, put 0 for all articles
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

// {{{ class ReportData
function ReportData() {
  this.num_nodes = 0;
  this.num_articles = 0;
  this.num_artlinks = 0;
  this.num_categories = 0;
  this.num_catlinks = 0;
  this.largest_scc = 0;  // cut-off point for closeness calculation
  this.articles_done = 0;
  this.reachable_sum = 0;
  this.distance_sum = 0;
  this.onevent = {all_done:function(){}};
  this.keep_closest = KEEP_CLOSEST;
  this.closest = new Heap();
  this.interesting_nodes = [];
};
ReportData.prototype.on = function(name, cb) {
  this.onevent[name] = cb;
};
ReportData.prototype.SetSCC = function(components) {
  this.scc = components;
  for(var i=components.length-1; i>=0; i--) {
    var size = components[i][0];
    var count = components[i][1];
    if(size > this.largest_scc) {
      this.largest_scc = size;
    }
  }
};
ReportData.prototype.SetPageRanks = function(ranks) {
  this.pageranks = ranks;
};
ReportData.prototype.AddArticle = function(node, count_dist) {
  var distance = 0;
  var reachable = 0;

  // 'count_dist' at index 'i' holds a count of how many nodes have min-distance
  // equal to 'i'.
  
  // Exclude node itself from reachable count: TODO(user) Is this correct?

  for (var d = 1, _len = count_dist.length; d < _len; d++) {
    var count = count_dist[d];
    // Accumulate total number of reachable nodes
    reachable += count;
    // Accumulate distances, this will be used to compute average distance.
    distance += count * d;
  }

  this.articles_done += 1;
  this.distance_sum += distance;
  this.reachable_sum += reachable;

  if(this.largest_scc == 0) {
    throw new Error("scc must be computed before adding articles");
  }

  // Only nodes that can reach largest SCC or more have right to be included
  // in computation of closeness centrality.
  if(reachable + 1 >= this.largest_scc) { // +1 is for node itself.
    var closeness = distance / reachable;
    this.closest.insert(-closeness, node);
    if(this.closest.size() > this.keep_closest) {
      this.closest.remove();
    }
  }

  if(!this.sample_size) {
    throw new Error("sample size is not set");
  }

  if(this.articles_done == this.sample_size) {
    this.onevent['all_done'](); // Call event
  };
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
  this.onevent = {job_complete:function(){}, stopped:function(){}};
}
JobMonitor.prototype.on = function(name, cb) {
  this.onevent[name] = cb;
};
JobMonitor.prototype.Start = function() {
  if (this.monitor)
    throw new Error("JobMonitor is already running");
  this.monitor = true;
  this._popqueue_block();
};
JobMonitor.prototype.Stop = function() {
  if (!this.monitor)
    throw new Error("JobMonitor is not started");
  this.monitor = false;
};
JobMonitor.prototype._popqueue_block = function() {
  var _this = this;
  this.redis_block.blpop('queue:running', 0, function(){
    _this._job_pop.apply(_this, arguments);
  });
};
JobMonitor.prototype._job_pop = function(err, result) {
  if (err) throw err;
  var job = result[1];

  var _this = this;
  var wait = this.waittime_job[job] || this.wait_milisec;

  setTimeout(function(){
    _this.redis.get('result:'+job, function(err,status) {
      if (err) throw err;
      if (status) {
        _this.onevent.job_complete(job,status);
      } else {
        // Job is lost, push it back into the run-queue
        _this.redis.lpush('queue:jobs',job);
        console.log('Re-scheduling job ' + job);
      }
    });
  }, wait);

  if (this.monitor) {
    this._popqueue_block();  // monitor queue for next job
  } else {
    this.onevent.stopped();  // call event
  }
};
// }}}

// {{{ class Controller
// Controller just adds new jobs to the queue,
// and calls the callback on completion. 
function Controller(redis, redis_pubsub) {
  this.redis = redis;
  this.redis_pubsub = redis_pubsub;
  this.onevent = {};
}
Controller.prototype.on = function(name, cb) {
  this.onevent[name] = cb;
};
Controller.prototype._RunJob = function(job, callback) {
  // First listen on a channel where results will be announced
  var _this = this;
  this.redis_pubsub.subscribeTo('announce:'+job, function(channel, msg) {
    _this.redis_pubsub.unsubscribe(channel);
    callback(job, JSON.parse(msg));
  });
  // Push the job on queue
  this.redis.lpush('queue:jobs', job);
};
Controller.prototype.RunJob = function(job, callback) {
  // Results are cached, check that first
  var _this = this;
  this.redis.get('result:'+job, function(err, result) {
    if(err) throw err;
    if (result) {
      callback(job, JSON.parse(result));
    } else {
      // Result is not in cache, issue a new job
      _this._RunJob(job, callback);
    }
  });
};
Controller.prototype.JobsForNodes = function(num_nodes, command, map) {
  var node = 0;
  // Slow-start
  var bulksize = 5;
  var granul = 2;
  var _this = this;
  
  map = map || function(i) { return i; };

  var insert_bulk = function() {
    _this.redis.llen('queue:jobs', function(err, len) {
      if (err) throw err;
      var percent = (100*(node-len)/num_nodes).toFixed(4);
      console.log(percent+'%  queue:jobs len is '+len+' bulksize '+bulksize);
      if (len < bulksize || len < granul) {
        bulksize += granul;
        var endpoint = Math.min(num_nodes, node + bulksize);
        for(var i=node+1; i<= endpoint; i++) {
          _this.redis.lpush('queue:jobs', command + map(i));
        }
        node += bulksize;
      } else {
        if(bulksize > 0) {
          bulksize -= granul;
        }
      }
      if (node < num_nodes || len > 0) {
        setTimeout(insert_bulk, 1000);
      } else {
        node = num_nodes;
        console.log('All jobs are inserted into the queue.');
      }
    });
  };
  insert_bulk();
};
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
Controller.prototype.ResolveInfo = function(total, cb_get_node, cb_set_info, cb_done) {
  if(!total) {
    cb_done();
  }
  var resolved = 0;
  var _this = this;
  // Resolve names of articles given list of nodes
  for (var i = 0; i < total; i++) {
    var node = cb_get_node(i);
    this.RunJob("I" + node, (function(){
      var _i = i; 
      var _node = node;
      return function(job, result) {
        if(result.error) throw result.error;
        _this.RunJob("D"+_node, function(job, dist) {
          if(!dist.error) {
            // Categories will report error, do this only for articles
            cb_set_info(_i, result.in_degree, result.out_degree, dist.count_dist);
          }
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
}
Mutex.prototype.Start = function(callback) {
  if (this.run)
    throw new Error("Mutex is already running");

  var _this = this;
  this.redis.incr('mutex', function(err, cnt1) {
    if (err) throw err;
    // Check if there is another instance running
    setTimeout(function(){
      _this.redis.incr('mutex', function(err, cnt2) {
        if (err) throw err;
        if(cnt1 + 1 == cnt2) {
          _this.run = true;
          _this._incr();
          callback();
        } else {
          throw new Error("Another instance is running");
        }
      });
    }, 2000);
  });
};
Mutex.prototype.Stop = function() {
  if (!this.run)
    throw new Error("Mutex was not started");
  this.run = false;
};
Mutex.prototype._incr = function() {
  var _this = this;
  setTimeout(function(){
    if(!_this.run) return;
    
    _this.redis.incr('mutex',function(err, cnt) {
      if (err) throw err;
      _this._incr();
    });
  }, 1000);
};
// }}}

// {{{ class Processing
function Processing() {
  this.jobs = Array.prototype.slice.call(arguments);
}
Processing.prototype.run = function(i) {
  i = i || 0;
  var jobs = this.jobs;
  if(i >= jobs.length) {
    return;
  }
  var _this = this;
  this._parallel(jobs[i], function() {
    _this.run(i+1);
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
function GroupPermutation(n) {
  this.n = n;
  this.s = 1 + Math.floor(Math.random()*n);
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

function d() {
  console.log(arguments);
}

function main() {
  var redis = redisnode.createClient(); // used for non-blocking commands
  var redis_block = redisnode.createClient(); // used for blocking command blpop
  var redis_pubsub = redisnode.createClient(); // used for pub/sub messages

  var data = new ReportData();
  var mutex = new Mutex(redis);
  var control = new Controller(redis, redis_pubsub);  // pubsub can't be mixed with regular ops
  var monitor = new JobMonitor(redis, redis_block);  // blpop is blocking operation
  monitor.Start();

  // Init step 0 - start mutex
  var init_mutex = function(callback) {
    mutex.Start(callback);
  };

  // Init step 1 - get counts
  var init_get_counts = function(callback) {
    redis.mget("s:count:Graph_nodes", "s:count:Articles", 
        "s:count:Article_links", "s:count:Categories",
        "s:count:Category_links",
    function(err, result) {
      data.num_nodes = parseInt(result[0], 10);
      data.num_articles = parseInt(result[1], 10);
      data.num_artlinks = parseInt(result[2], 10);
      data.num_categories = parseInt(result[3], 10);
      data.num_catlinks = parseInt(result[4], 10);
      callback();
    });
  };

  // Init step 2 - compute SCC
  var init_compute_scc = function(callback) {
    control.RunJob('S', function(job, result) {
      data.SetSCC(result.components);
      callback();
    });
  };

  // Init step 3 - get PageRanks
  //monitor.waittime_job['R'] = 30 * 1000; // this job may take longer time than normal
  var init_compute_pageranks = function(callback) {
    control.RunJob('R', function(job, result) {
      var ranks = result.ranks;
      for(var i = 0; i < ranks.length; i++) {
        data.interesting_nodes.push({node:ranks[i][1]});
      }
      control.ResolveNames(ranks.length, 
        function get(i) {
          return ranks[i][1];
        },
        function set(i, name) {
          ranks[i][2] = name;
        },
        function done() {
          data.SetPageRanks(ranks);
          callback();
        }
      );
    });
  };

  var compute_article_distances = function(callback) {
    assert.strictEqual(data.articles_done, 0);

    // Calculate count of distance between articles
    monitor.on('job_complete', function(job, result) {
      if (job[0] != 'D') {
        return; // only care about article distances
      }
      var r = JSON.parse(result)
      if (r.error) return;
      
      assert.strictEqual(r.count_dist[0], 1);

      var node = parseInt(job.substr(1), 10);
      data.AddArticle(node, r.count_dist);
    });

    data.on('all_done', callback);  // Articles

    if(!RANDOM_ARTICLES || RANDOM_ARTICLES >= data.num_nodes) {
      // Process every node
      data.sample_size = data.num_articles;
      control.JobsForNodes(data.num_nodes, 'D');
    } else {
      // Randomly select RANDOM_ARTICLES from a list
      data.sample_size = RANDOM_ARTICLES;
      var rand = new GroupPermutation(data.num_nodes);
      control.JobsForNodes(RANDOM_ARTICLES, 'D', function(i) {
        return rand.get(i);
      });
    }
  };

  var centr_articles = function(callback) {
    // Prepare article distance centers for report
    var pos = data.closest.size();
    data.close_centr = [];
    while(pos) {
      var heapnode = data.closest.remove();
      pos -= 1;
      data.interesting_nodes.push({node:heapnode.value});
      data.close_centr[pos] = {
        node:heapnode.value,
        avg_dist: - heapnode.key
      };
    }
    callback();
  };
  
  var resolve_centr_names = function(callback) {
    control.ResolveNames(data.close_centr.length,
      function get(i) {
        return data.close_centr[i].node;
      },
      function set(i, name) {
        data.close_centr[i].name = name;
      },
      callback
    );
  }

  var simplify_interesing = function(callback) {
    var list = data.interesting_nodes;

    list = list.sort(function (a, b) { return a.node - b.node; });
    var ret = [list[0]];
    for (var i = 1; i < list.length; i++) {
      if (list[i-1].node !== list[i].node) {
        ret.push(list[i]);
      }
    }
    data.interesting_nodes = ret;
    callback();
  };

  var resolve_interesting_names = function(callback) {
    control.ResolveNames(data.interesting_nodes.length,
      function get(i) {
        return data.interesting_nodes[i].node;
      },
      function set(i, name) {
        data.interesting_nodes[i].name = name;
      },
      callback
    );
  };

  var resolve_interesting_info = function(callback) {
    control.ResolveInfo(data.interesting_nodes.length,
      function get(i) {
        return data.interesting_nodes[i].node;
      },
      function set(i, in_degree, out_degree, count_dist) {
        var node_data = data.interesting_nodes[i]

        // For categories, this function will not be called
        node_data.is_article = true;
        
        node_data.in_degree = in_degree;
        node_data.out_degree = out_degree;
        node_data.count_dist = count_dist;
        node_data.max_dist = Math.max.apply(null, count_dist);

        node_data.reachable = 0;
        node_data.distance = 0;
        for (var d = 1, _len = count_dist.length; d < _len; d++) {
          var count = count_dist[d];
          node_data.reachable += count;
          node_data.distance += count * d;
        }
      },
      callback
    );
  };

  var stop_and_close = function(callback) {
    monitor.Stop();
    mutex.Stop();

    redis.close();
    redis_block.close();
    redis_pubsub.close();
    console.log(data);

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

  // Complete description of report generation process
  var process = new Processing(

      [init_mutex, init_get_counts, init_compute_scc, init_compute_pageranks],

      [compute_article_distances],

      [centr_articles],
      
      [resolve_centr_names, simplify_interesing],

      [resolve_interesting_names, resolve_interesting_info],

      [stop_and_close, write_report]
  );
  process.run();
};

main();

