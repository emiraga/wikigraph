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

#include "hiredis/hiredis.h"
#include "config.h"

typedef pair<int, int> pii;

namespace wikigraph {


void print_help(char *prg) {
  printf("Usage: %s [options]\n", prg);
  printf("\n");
  printf("-f N\tStart N background workers\n");
  printf("-r HOST\tName of the host of redis server, default:%s\n", REDISHOST);
  printf("-p PORT\tPort of redis server, default:%d\n", REDISPORT);
  printf("-h\tShow this help\n");
  printf("\n");
  printf("Visit https://github.com/emiraga/wikigraph for more info.\n");
}

}  // namespace wikigraph

int main(int argc, char *argv[]) {
  int fork_off = 0;

  char redis_host[51] = REDISHOST;
  int redis_port = REDISPORT;

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

  // Should be called before fork()'s to save memory (copy-on-write)
  load_graph();

  bool is_parent = true;
  if (fork_off) {
    for (int i = 1; i < fork_off; i++) {
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

  redisContext *c;
  struct timeval timeout = { 1, 500000 };  // 1.5 seconds
  c = redisConnectWithTimeout(redis_host, redis_port, timeout);
  if (c->err) {
    printf("Connection error: %s\n", c->errstr);
    exit(1);
  }

  redisReply *reply = (redisReply*)redisCommand(c, "GET s:count:Graph_nodes");
  assert(reply->type == REDIS_REPLY_STRING);

  num_nodes = atoi(reply->str);
  freeReplyObject(reply);

  if (is_parent) {
    printf("Number of nodes %d\n", num_nodes);
  }

  // Assure that we don't exceed maximum number of nodes
  assert(num_nodes < MAXNODES);

  while (1) {
    // Wait for a job on the queue
    redisReply *reply = (redisReply*)redisCommand(c,"BRPOPLPUSH queue:jobs queue:running 0");

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
    switch (job[0]) {
      case 'D': {  // count distances from node
#ifdef DEBUG
        // Simulate slow processing
        usleep(1000 * 100);
#endif
        int node = atoi(job+1);
        if (node < 1 || node > num_nodes) {
          result = "{\"error\":\"Node out of range\"}";
          break;
        }
        if (IS_CATEGORY(node)) {
          result = "{\"error\":\"Node is category\"}";
          break;
        }
        vector<int> cntdist = get_distances(node);
        result = "{\"count_dist\":" + to_json(cntdist) + "}";
      }
      break;
      case 'S': {  // Sizes of strongly connected components
        vector<pii> components = scc_tarjan();
        result = "{\"components\":" + to_json(components) + "}";
      }
      break;
      case 'P': {  // PING 'Are you still there?' (in GLaDOS voice)
        result = "{\"still_alive\":\"This was a triumph.\"}";
      }
      break;
#ifdef DEBUG
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

    /* Set results */
    reply = (redisReply*)redisCommand(c, "SETEX result:%s %d %b",
        job, RESULTS_EXPIRE, result.c_str(), result.size());
    freeReplyObject(reply);

    /* Announce the results to channel */
    reply = (redisReply*)redisCommand(c,"PUBLISH announce:%s %b",
        job, result.c_str(), result.size());
    freeReplyObject(reply);

    if (is_parent) {
      time_t t_end = clock();
      printf("Time to complete %.5lf: %s\n", 
          double(t_end - t_start)/CLOCKS_PER_SEC, result.c_str());
    }
  }

  return 0;
}

