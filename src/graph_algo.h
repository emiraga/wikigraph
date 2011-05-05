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
#include <climits>
#include <cmath>

#include "graph.h"

namespace wikigraph {

class CompleteGraphAlgo {
  static const int DIST_ARRAY = 100;  // threshold to use array for distances
 public:
  explicit CompleteGraphAlgo(File *file)
  : file_(file), invalid_node_(NULL), queue_(NULL) {
    graph_.list = NULL;
    graph_.edges = NULL;
  }

  CompleteGraphAlgo(File *file, BitArray *valid_node)
  : file_(file), invalid_node_(valid_node), queue_(NULL) {
    graph_.list = NULL;
    graph_.edges = NULL;
  }

  void Init() {
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
    if (graph_.edges)
      delete[] graph_.edges;
    if (graph_.list)
      delete[] graph_.list;
    if (queue_) {
      delete[] queue_;
      delete[] dist_;
    }
  }

  vector<uint32_t> GetDistances(node_t start) {
    uint32_t dist_count[DIST_ARRAY] = {0};
    std::map<uint32_t, uint32_t> dist_count_m;

    assert(invalid_node_ == NULL || invalid_node_->get_value(start) == false);

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
          // Visit new node
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

  vector<uint32_t> Scc() {  // Tarjan
    int tindex = 1;
    int top = -1;
    int top_scc = -1;
    const int MAXNODES = graph_.num_nodes + 2;
    const int num_nodes = graph_.num_nodes;

    int32_t *lowindex = dist_;  // reuse some memory
    int32_t *nodeindex = new int32_t[MAXNODES];
    memset(nodeindex, -1, 4 * (MAXNODES));
    int8_t *instack = new int8_t[MAXNODES];
    memset(instack, 0, MAXNODES);
    int32_t *stackI = new int32_t[MAXNODES];
    memset(stackI, 0, MAXNODES);
    int32_t *stacknode = new int32_t[MAXNODES];
    memset(stacknode, 0, MAXNODES);
    node_t *stack = queue_;  // reuse some memory

    vector<uint32_t> scc_result;

    for (int k = 1; k <= num_nodes; k++) {
#ifdef DEBUG
      printf("processing node %d\n", k);
#endif
      if(invalid_node_ && invalid_node_->get_value(k))
        continue;

      // This node should not be previously visited /*and not be a category*/
      if (nodeindex[k] == -1  /* && !IS_CATEGORY(k)*/) {
        // Push to execution stack
        stacknode[++top] = k;
        stackI[top] = -1;

        // Process stack
        while (top >= 0) {
          int node = stacknode[top];
          int &i = stackI[top];

          if (i == -1) {  // Uninitialized
            // Initialize the node
            nodeindex[node] = tindex++;
            lowindex[node] = nodeindex[node];

            stack[++top_scc] = node;
            instack[node] = true;
            i = graph_.start(node);  // node_begin_list[node];
          } else if (i < graph_.end(node)) {  // node_end_list(node)) {
            // New location of REF1 (see bellow)
            int dest = graph_.edges[i];
            lowindex[node] = std::min(lowindex[node], lowindex[dest]);
            i++;
          }
          for ( ; i < graph_.end(node) /* node_end_list(node) */; i++) {
            int dest = graph_.edges[i];
            if (nodeindex[dest] == -1) {  // Not visited
              // Push dest to execution stack
              stacknode[++top] = dest;
              stackI[top] = -1;
              break;  // Original REF1 code
            } else if (instack[dest]) {  // does belongs to this component
              lowindex[node] = std::min(lowindex[node], lowindex[dest]);
            }
          }
          if (i == graph_.end(node)) {  // node_end_list(node)) {
            if (lowindex[node] == nodeindex[node]) {
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
              } while (sccnode != node);
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

    // return util::count_items(scc_result);
    return scc_result;
  }

  vector<pair<double, node_t> > PageRank(uint32_t how_many, bool verbose) {
    double *rank1 = new double[graph_.num_nodes + 2];
    double *rank2 = new double[graph_.num_nodes + 2];

    uint32_t N = 0u;
    for (node_t node = 1; node <= graph_.num_nodes; node++) {
      if (invalid_node_ && invalid_node_->get_value(node))
        continue;

      N++;
    }

    const double dumping = 0.85;
    for (node_t node = 1; node <= graph_.num_nodes; node++) {
      if (invalid_node_ && invalid_node_->get_value(node))
        continue;

      rank1[node] = 1.0 / N;
    }

    while (true) {
      PowerIteration(N, dumping, rank1, rank2);
      std::swap(rank1, rank2);
      // rank1 contains results

      double delta = 0.0;
      for (node_t node = 1; node <= graph_.num_nodes; node++) {
        if (invalid_node_ && invalid_node_->get_value(node))
          continue;

        delta += std::fabs(rank1[node] - rank2[node]);
      }
      if (verbose) {
        printf("Delta: %lf\n", delta);
      }

      if (delta < 1e-3)
        break;
    }

    double ranksum = 0.0;
    for (node_t node = 1; node <= graph_.num_nodes; node++) {
      if (invalid_node_ && invalid_node_->get_value(node))
        continue;

      ranksum += rank1[node];
    }

    vector<pair<double, node_t> > ret;

    for (node_t node = 1; node <= graph_.num_nodes; node++) {
      if (invalid_node_ && invalid_node_->get_value(node))
        continue;

      ret.push_back(std::make_pair(rank1[node], node));
    }
    delete[] rank1;
    delete[] rank2;

    how_many = std::min(how_many, N);
    std::partial_sort(ret.begin(), ret.begin() + how_many, ret.end(),
        std::greater< pair<double, node_t> >());
    ret.resize(how_many);
    for (size_t i = 0; i < ret.size(); i++) {
      ret[i].first /= ranksum;
    }
    return ret;
  }

  pii DegreeInfo(node_t info_node) {
    uint32_t out_degree = graph_.end(info_node) - graph_.start(info_node);
    uint32_t in_degree = 0u;

    for (node_t node = 1; node <= graph_.num_nodes; node++) {
      if (invalid_node_ && invalid_node_->get_value(node))
        continue;

      node_t *target = &graph_.edges[graph_.start(node)];
      node_t *end = &graph_.edges[graph_.end(node)];
      for ( ; target < end; target++) {
        // link is from (node) to (*target)

        if(*target == info_node) {
          in_degree++;
        }

        // Checking the sanity of graph
        assert(invalid_node_ == NULL || invalid_node_->get_value(*target) == false);
      }
    }
    return pii(in_degree, out_degree);
  }

  void SanityCheck() {
    for (node_t node = 1; node <= graph_.num_nodes; node++) {
      node_t *target = &graph_.edges[graph_.start(node)];
      node_t *end = &graph_.edges[graph_.end(node)];
      for ( ; target < end; target++) {
        // link is from (node) to (*target)
 
        // Checking that there are no links to invalid nodes
        assert(invalid_node_ == NULL || invalid_node_->get_value(*target) == false);
      }
    }
  }

  uint32_t num_nodes() const {
    return graph_.num_nodes;
  }

  uint32_t num_edges() const {
    return graph_.num_edges;
  }

 protected:
  void PowerIteration(uint32_t N, double dumping,
      double *rank_in, double *rank_out) {
    for (node_t node = 1; node <= graph_.num_nodes; node++) {
      if (invalid_node_ && invalid_node_->get_value(node))
        continue;

      rank_out[node] = (1.0 - dumping) / N;
    }
    for (node_t node = 1; node <= graph_.num_nodes; node++) {
      if (invalid_node_ && invalid_node_->get_value(node))
        continue;

      node_t *target = &graph_.edges[graph_.start(node)];
      node_t *end = &graph_.edges[graph_.end(node)];
      int out_degree = graph_.end(node) - graph_.start(node);

      double contrib_rank = dumping * rank_in[node] / out_degree;
      for ( ; target < end; target++) {
        // link is from (node) to (*target)
        rank_out[*target] += contrib_rank;
      }
    }
  }

 private:
  File *file_;
  Graph graph_;
  BitArray *invalid_node_;

  // Used in computation
  node_t *queue_;
  int32_t *dist_;
 private:
  DISALLOW_COPY_AND_ASSIGN(CompleteGraphAlgo);
};

}  // namespace wikigraph

#endif  // SRC_GRAPH_ALGO_H_

