// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_GRAPH_ALGO_H_
#define SRC_GRAPH_ALGO_H_

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include <map>
#include <algorithm>
#include <utility>

#include "graph.h"

namespace wikigraph {

typedef std::pair<uint32_t, uint32_t> pii;

class CompleteGraphAlgo {
  File *file_;
  Graph graph_;

  // Used in computation
  node_t *queue_;
  int32_t *dist_;

  static const int DIST_ARRAY = 100; // threshold to use array for distances
 public:
  CompleteGraphAlgo(File *file)
  : file_(file), queue_(NULL) {
    graph_.list = NULL;
    graph_.edges = NULL;
  }
  void init() {
    assert(graph_.list == NULL);
    node_t tmp[2];
    // Read from back
    file_->seek(-off_t(sizeof(uint32_t) * 2), SEEK_END);
    file_->read(tmp, sizeof(uint32_t), 2);
    // Return back to beginning
    file_->seek(0, SEEK_SET);

    graph_.num_edges = tmp[0];
    graph_.num_nodes = tmp[1];

    // Read edges
    graph_.edges = new uint32_t[ graph_.num_edges ];
    file_->read(graph_.edges, sizeof(uint32_t), graph_.num_edges);

    // Read list of nodes (+2 for index zero and extra element at end.)
    graph_.list = new uint32_t[ graph_.num_nodes + 2 ];
    file_->read(graph_.list, sizeof(uint32_t), graph_.num_nodes + 2);

    // For processing
    queue_ = new uint32_t[ graph_.num_nodes + 2];
    dist_ = new int32_t[ graph_.num_nodes + 2];
  }
  ~CompleteGraphAlgo() {
    if(graph_.edges)
      delete[] graph_.edges;
    if(graph_.list)
      delete[] graph_.list;
    if(queue_) {
      delete[] queue_;
      delete[] dist_;
    }
  }
  vector<uint32_t> get_distances(node_t start) {
    uint32_t dist_count[DIST_ARRAY] = {0};
    std::map<uint32_t, uint32_t> dist_count_m;

    memset(dist_, -1, sizeof(dist_[0])*(graph_.num_nodes + 2));
    dist_[start] = 0;
    dist_count[0]++;

    // Add start node to queue
    queue_[0] = start;
    int queuesize = 1;
    // Process nodes from queue
    for (int top = 0; top < queuesize; top++) {
      node_t node = queue_[top];

      // Loop over all neighbours of node
      node_t *target = &graph_.edges[graph_.start(node)];
      node_t *end = &graph_.edges[graph_.end(node)];
      for ( ; target < end; target++) {
        int32_t &dist_target = dist_[*target];
        if (dist_target == -1) {
          dist_target = dist_[node] + 1;
          queue_[queuesize++] = *target;

          if (dist_target < DIST_ARRAY)
            dist_count[dist_target]++;
          else
            dist_count_m[dist_target]++;
        }
      }
    }

    vector<uint32_t> result;
    for (int i = 0; i < DIST_ARRAY; i++) {
      if (!dist_count[i])
        return result;

      result.push_back(dist_count[i]);
    }
    for (std::map<uint32_t, uint32_t>::iterator i = dist_count_m.begin();
        i != dist_count_m.end(); i++) {
      result.push_back(i->second);
    }
    return result;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(CompleteGraphAlgo);
};

namespace util {

vector<pii> count_items(vector<uint32_t> v) {
  sort(v.begin(), v.end());
  v.push_back(INT_MAX);
  vector<pii> result;

  int cnt = 0;
  for (size_t i = 0; i < v.size(); i++) {
    if (i && v[i-1] != v[i]) {
      result.push_back(pii(v[i-1], cnt));
      cnt = 1;
    } else {
      cnt++;
    }
  }
  return result;
}

string to_json(const vector<uint32_t> &v) {
  string msg = "[";
  for(size_t i = 0; i < v.size(); i++) {
    if(i) msg += ",";
    char msgpart[21];
    snprintf(msgpart, 20, "%"PRIu32, v[i]);
    msg += string(msgpart);
  }
  msg += "]";
  return msg;
}

string to_json(const vector<pii> &v) {
  string msg = "[";
  for(size_t i=0; i<v.size(); i++) {
    if(i) msg += ",";
    char msgpart[41];
    snprintf(msgpart, 40, "[%"PRIu32",%"PRIu32"]", v[i].first, v[i].second);
    msg += string(msgpart);
  }
  msg += "]";
  return msg;
}

}  // namespace util

}  // namespace wikigraph

//int* get_int_array(int size, int init) {
//  int* res = (int*)malloc(sizeof(int)*size);
//  if(!res) {
//    fprintf(stderr, "malloc error\n");
//    exit(1);
//  }
//  memset(res, init, sizeof(int)*size);
//  return res;
//}

#endif  // SRC_GRAPH_ALGO_H_

// Copyright 2011 Emir Habul, see file COPYING

#if 0
vector<pii> scc_tarjan() {
  int tindex = 1;
  int top = -1;
  int top_scc = -1;

  int *lowindex = dist;  // reuse some memory
  int *nodeindex = get_int_array(MAXNODES, -1);
  int *instack = get_int_array(MAXNODES, 0);
  int *stackI = get_int_array(MAXNODES, 0);
  int *stacknode = get_int_array(MAXNODES, 0);
  int *stack = queue;  // reuse some memory

  vector<int> scc_result;

  for(int k=1; k<=num_nodes; k++) {
#ifdef DEBUG
    printf("processing node %d\n", k);
#endif
    // This node should not be previously visited and not be a category
    if(nodeindex[k] == -1 && !IS_CATEGORY(k)) {
      if(k&255 == 0)printf("%d\n", k);
      // Push to execution stack
      stacknode[++top] = k;
      stackI[top] = -1;

      // Process stack
      while(top >= 0) {
        int node = stacknode[top];
        int &i = stackI[top];

        if(i == -1) {  // Uninitialized
          // Initialize the node
          nodeindex[node] = tindex++;
          lowindex[node] = nodeindex[node];

          stack[++top_scc] = node;
          instack[node] = true;
          i = node_begin_list[node];
        } else if(i < node_end_list(node)) {
          // New location of REF1 (see bellow)
          int dest = edges[i];
          lowindex[node] = min(lowindex[node], lowindex[dest]);
          i++;
        }
        for( ; i < node_end_list(node); i++) {
          int dest = edges[i];
          if(nodeindex[dest] == -1) { // Not visited
            // Push dest to execution stack
            stacknode[++top] = dest;
            stackI[top] = -1;
            break;  // Original REF1 code
          }
          else if (instack[dest]) { //dest belongs to this component
            lowindex[node] = min(lowindex[node], lowindex[dest]);
          }
        }
        if(i == node_end_list(node)) {
          if(lowindex[node] == nodeindex[node]) {
            // We found one component
            int count = 0;
#ifdef DEBUG
            printf("Component:[");
#endif
            int sccnode;
            do {
              sccnode = stack[top_scc--];
              instack[sccnode] = false;
              count++;

#ifdef DEBUG
              printf(" %d", sccnode);
#endif
            } while(sccnode != node);
#ifdef DEBUG
            printf(" ]\n");
#endif
            scc_result.push_back(count);
          }
          // Remove node from execution stack
          top--;
        }
      }
      assert(top_scc == -1);
    }
  }
  // free(lowindex);
  free(nodeindex);
  free(instack);
  free(stackI);
  free(stacknode);
  // free(stack);

  return count_items(scc_result);
}
#endif

