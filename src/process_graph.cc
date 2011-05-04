// Copyright 2011 Emir Habul, see file COPYING

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cassert>
#include <climits>

#include <getopt.h>
#include <unistd.h>
#include <time.h>

#include <algorithm>
#include <map>
#include <string>

#include "config.h"
#include "wikigraph_stubs_internal.h"
#include "redis.h"
#include "file_io.h"
#include "graph_algo.h"

namespace wikigraph {

void print_help(char *prg) {
  printf("Usage: %s [options]\n", prg);
  printf("\n");
  printf("-f N\tStart N background workers\n");
  printf("-r HOST\tName of the host of redis server, default:%s\n", REDIS_HOST);
  printf("-p PORT\tPort of redis server, default:%d\n", REDIS_PORT);
  printf("-h\tShow this help\n");
  printf("\n");
  printf("Visit https://github.com/emiraga/wikigraph for more info.\n");
}

string graph_command(char *job, node_t node, CompleteGraphAlgo *graph, uint32_t num_nodes) {
  string result;
  switch (job[0]) {
    case 'D': {  // count distances from node
      vector<uint32_t> cntdist = graph->GetDistances(node);
      result = "{\"count_dist\":" + util::to_json(cntdist) + "}";
    }
    break;
    case 'S': {  // Sizes of strongly connected components
      vector<pii> components = util::count_items(graph->Scc());
      result = "{\"components\":" + util::to_json(components) + "}";
    }
    break;
    case 'I': {  // Degree info
      pii degrees = graph->DegreeInfo(node);
      char msg[50];
      snprintf(msg, sizeof(msg), "{\"in_degree\":%"PRIu32",\"out_degree\":%"PRIu32"}",
          degrees.first, degrees.second);
      result = string(msg);
    }
    break;
    case 'R': {  // Page Rank
      vector<pair<double, node_t> > rankp =
        graph->PageRank(PAGERANK_RESULTS);
      result = "{\"ranks\":" + util::to_json(rankp) + "}";
    }
    break;
    default:
      result = "{\"error\":\"Unknown command\"}";
  }
  return result;
}

int main(int argc, char *argv[]) {
  int fork_off = 0;

  char redis_host[51] = REDIS_HOST;
  int redis_port = REDIS_PORT;

  while (1) {
    int option = getopt(argc, argv, "f:r:p:h");
    if (option == -1)
      break;
    switch (option) {
      case 'f':
        fork_off = atoi(optarg);
      break;
      case 'r':
        strncpy(redis_host, optarg, 50);
      break;
      case 'p':
        redis_port = atoi(optarg);
      break;
      case 'h':
        print_help(argv[0]);
        return 0;
      break;
      default:
        print_help(argv[0]);
        return 1;
    }
  }
  if (argc != optind) {
    print_help(argv[0]);
    return 1;
  }

  // Next three sections should be called before fork()'s to 
  // save memory (copy-on-write)

  // Load category links
  SystemFile f_cat;
  if (!f_cat.open("catlinks.graph", "rb")) {
    perror("fopen");
    exit(1);
  }
  CompleteGraphAlgo cat_graph(&f_cat);
  cat_graph.Init();
  f_cat.close();

  // Check sanity of graph
  cat_graph.DegreeInfo(1);

  // Load bit array => is node a category
  BitArray is_category(cat_graph.num_nodes()+1);
  SystemFile f_iscat;
  if (!f_iscat.open("graph_nodeiscat.bin", "rb")) {
    perror("fopen");
    exit(1);
  }
  is_category.loadFile(&f_iscat);
  f_iscat.close();

  // Load article links
  SystemFile f_art;
  if (!f_art.open("artlinks.graph", "rb")) {
    perror("fopen");
    exit(1);
  }
  CompleteGraphAlgo art_graph(&f_art, &is_category);  // Category nodes are invalid
  art_graph.Init();
  f_art.close();

  // Check sanity of graph
  art_graph.SanityCheck();

  // Forking children into background
  bool is_parent = true;
  if (fork_off) {
    for (int i = 0; i < fork_off; i++) {
      if (!fork()) {
        is_parent = false;
        break;
      }
    }
    if (is_parent) {
      printf("Started %d worker(s).\n", fork_off);
      return 0;
    }
  }

  // Connect to redis server over network (not unix-socket)
  redisContext *c;
  struct timeval timeout = { 1, 500000 };  // 1.5 seconds
  c = redisConnectWithTimeout(redis_host, redis_port, timeout);
  if (c->err) {
    printf("Connection error: %s\n", c->errstr);
    exit(1);
  }

  redisReply *reply = redisCmd(c, "GET s:count:Graph_nodes");
  assert(reply->type == REDIS_REPLY_STRING);

  uint32_t num_nodes = atoi(reply->str);
  freeReplyObject(reply);

  if (num_nodes != art_graph.num_nodes()
      || num_nodes != cat_graph.num_nodes()) {
    fprintf(stderr, "Number of nodes mismatch.\n");
    exit(1);
  }

  if (is_parent) {
    printf("Number of nodes %d\n", num_nodes);
  }

  while (1) {
    // Wait for a job on the queue
    redisReply *reply = redisCmd(c, "BRPOPLPUSH queue:jobs queue:running 0");

    char job[101];
    strncpy(job, reply->str, 100);
    job[100] = 0;
    freeReplyObject(reply);

    if (is_parent) {
      printf("Request: %s\n", job);
    }

    time_t t_start = clock();
    string result;
    bool no_result = false;

    switch(job[0]) {
      // command
      case 'a': { // for articles graph
        node_t node = 0;
        if(isdigit(job[2])) {
          node = atoi(job+2);
          if (node < 1 || node > num_nodes) {
            result = "{\"error\":\"Node out of range\"}";
            break;
          }
          if (is_category.get_value(node)) {
            result = "{\"error\":\"Node is category\"}";
            break;
          }
        }
        printf("node=%d\n", node);
        result = graph_command(job+1, node, &art_graph, num_nodes);
      }
      break;
      // command
      case 'c': { // for categories graph
        node_t node = 0;
        if(isdigit(job[2])) {
          node = atoi(job+2);
          if (node < 1 || node > num_nodes) {
            result = "{\"error\":\"Node out of range\"}";
            break;
          }
          // Category graph does not have limitation on which nodes it can be
          // called.
        }
        result = graph_command(job+1, node, &cat_graph, num_nodes);
      }
      break;
      // command
      case 'P': {  // PING 'Are you still there?'
        result = "{\"still_alive\":\"This was a triumph.\"}";
      }
      break;
#ifdef DEBUG
      // command
      case '.': {  // Job that does not produce any result
        no_result = true;
        // Used to test a crashing client
      }
      break;
#endif
      default:
        result = "{\"error\":\"Unknown command\"}";
    }
    if (no_result)
      continue;

    // Set results
    reply = redisCmd(c, "SETEX result:%s %d %b",
        job, RESULTS_EXPIRE, result.c_str(), result.size());
    freeReplyObject(reply);
    // Announcing must come after settings the results.

    // Announce the results to channel
    reply = redisCmd(c, "PUBLISH announce:%s %b",
        job, result.c_str(), result.size());
    freeReplyObject(reply);

    if (is_parent) {
      time_t t_end = clock();
      printf("Time to complete %.5lf: %s\n",
          static_cast<double>(t_end - t_start)/CLOCKS_PER_SEC, result.c_str());
    }
  }

  return 0;
}  // main

}  // namespace wikigraph

int main(int argc, char *argv[]) {
  return wikigraph::main(argc, argv);
}

