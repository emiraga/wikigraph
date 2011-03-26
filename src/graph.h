// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_GRAPH_H_
#define SRC_GRAPH_H_

#include <stddef.h>
#include <stdint.h>

#include "wikigraph-stubs-internal.h"

namespace wikigraph {

class BitArray {
 public:
  explicit BitArray(size_t size) : size_(size) {
    array_ = new uint32_t[(size+31)/32];
    // It is initialized to zero
    memset(array_, 0, sizeof(uint32_t)*((size+31)/32));
  }
  bool inline get_value(size_t x) {
    assert(x < size_);
    return array_[x/32] & (1 << (x % 32));
  }
  void inline set_true(size_t x) {
    assert(x < size_);
    array_[x/32] |= (1 << (x % 32));
  }
  void inline set_false(size_t x) {
    assert(x < size_);
    array_[x/32] &= ~(1 << (x % 32));
  }
 private:
  uint32_t *array_;
  uint32_t size_;
  DISALLOW_COPY_AND_ASSIGN(BitArray);
};

// Structure for storing complete graph in memory
//
// (in some cases list == NULL - to save memory
// edge list is read from memory)
//
struct Graph {
  node_t *edges;  // edge list contains list of nodes
  uint32_t *list;  // indices of edge list

  uint32_t num_edges, num_nodes;

  // start(node) is the index of beginning of edge list for node
  inline uint32_t& start(node_t node) {
    return list[2 * node];
  }
  // end(node) one element past the list end
  inline uint32_t& end(node_t node) {
    return list[2 * node + 1];
  }

  void release() {
    if (edges) {
      delete[] edges;
      edges = NULL;
    }
    if (list) {
      delete[] list;
      list = NULL;
    }
  }
};

struct NodeStream {
  node_t id;
  vector<node_t> list;  // Adjacency list
};

template<class FileReader>
class StreamGraphReader {
 public:
  explicit StreamGraphReader(FileReader *f)
  :file_(f), cur_node_(1) { }

  void init() {
    node_t tmp[2];
    file_->read_from_back(tmp, 2);
    graph_.num_edges = tmp[0];
    graph_.num_nodes = tmp[1];

    graph_.edges = NULL;  // They will be streamed

    // read +2 additional uint32 for num_edges and num_nodes
    size_t list_len = 2 * (graph_.num_nodes + 1) + 2;

    // Allocate memory for nodes
    graph_.list = new uint32_t[ list_len ];
    memory_allocated_ = list_len * 4;

    // Read list of nodes
    file_->read_from_back(graph_.list, list_len);
  }

  void next_node(NodeStream *node) {
    int len;
    do {
      len = graph_.end(cur_node_) - graph_.start(cur_node_);
      node->id = cur_node_;
      cur_node_++;
    }
    while (len == 0);

    // This can be optimized, but it is not frequently used
    node->list.clear();
    for (int i = 0; i < len; i++) {
      node->list.push_back(file_->read_unit());
    }
  }

  bool has_next() {
    return cur_node_ <= graph_.num_nodes;
  }

 protected:
  off_t memory_allocated_;
 private:
  Graph graph_;
  FileReader *file_;
  node_t cur_node_;
 private:
  DISALLOW_COPY_AND_ASSIGN(StreamGraphReader);
};

}  // namespace wikigraph

#endif  // SRC_GRAPH_H_

