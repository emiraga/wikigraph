// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_GRAPH_H_
#define SRC_GRAPH_H_

#include <stddef.h>
#include <stdint.h>

#include "wikigraph_stubs_internal.h"

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

// class GraphWriter
//
// FileWriter : concrete class BufferedWriter
template<class FileWriter>
class GraphWriter {
 public:
  GraphWriter(FileWriter *f, int num_nodes)
      : writer_(f), nodes_(num_nodes), cur_node_(0), file_pos_(0) {
    size_t list_len = (num_nodes + 2);
    list_ = new node_t[ list_len ];
    memset(list_, 0, sizeof(node_t) * list_len);
  }
  ~GraphWriter() {
    finish();
  }
  // Tell the class that you are beginning to emit edges for a new node
  void start_node(node_t node) {
    assert(node > 0);
    assert(node <= nodes_);
    if (node == cur_node_)
      return;
    assert(node > cur_node_);  // Nodes must be given in increasing order
    while (PREDICT_FALSE(++cur_node_ < node)) {
      start(cur_node_) = file_pos_;
    }
    start(node) = file_pos_;
  }
  void add_edge(node_t edge) {
    assert(edge > 0);
    assert(edge <= nodes_);

    assert(cur_node_ != 0);
    writer_->write_uint(edge);
    file_pos_++;
  }
  void add_edges(const vector<node_t> &edges) {
    assert(cur_node_ != 0);
    for (size_t i = 0; i < edges.size(); i++) {
      assert(edges[i] > 0);
      assert(edges[i] <= nodes_);
      writer_->write_uint(edges[i]);
      file_pos_++;
    }
  }
  void finish() {
    if (writer_ == NULL)
      return;

    while (PREDICT_TRUE(++cur_node_ <= nodes_)) {
      start(cur_node_) = file_pos_;
    }
    end(nodes_) = file_pos_;

    int num_edges = file_pos_;
    size_t list_len = (nodes_ + 2);
    writer_->write(list_, 4, list_len);  // write list of nodes
    writer_->write_uint(num_edges);  // to the end of file, number of edges
    writer_->write_uint(nodes_);  // and number of nodes are added

    writer_ = NULL;
  }
 private:
  // Refers to beginning of edge list for each node
  node_t& start(node_t node) {
    return list_[node];
  }
  // End of edge list for node
  node_t& end(node_t node) {
    return list_[node + 1];
  }

  FileWriter *writer_;
  uint32_t nodes_;  // number of nodes
  node_t cur_node_;  // which node is currently active
  uint32_t file_pos_;  // nodes/edges not bytes
  node_t *list_;  // beginning and end of edge list for each node
  // TODO(user) use struct Graph for this
 private:
  DISALLOW_COPY_AND_ASSIGN(GraphWriter);
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
  inline uint32_t start(node_t node) {
    return list[node];
  }
  // end(node) one element past the lists end
  inline uint32_t end(node_t node) {
    return list[node + 1];
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

// class StreamGraphReader
//
// FileReader : concrete class BufferedReader
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
    size_t list_len = (graph_.num_nodes + 2) + 2;

    // Allocate memory for nodes
    graph_.list = new uint32_t[ list_len ];
    memory_allocated_ = list_len * sizeof(uint32_t);

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

    // This can be optimized, it is not frequently used
    node->list.clear();
    for (int i = 0; i < len; i++) {
      node->list.push_back(file_->read_unit());
    }
  }

  bool has_next() {
    return cur_node_ <= graph_.num_nodes;
  }

 protected:
  size_t memory_allocated_;
 private:
  Graph graph_;
  FileReader *file_;
  node_t cur_node_;
 private:
  DISALLOW_COPY_AND_ASSIGN(StreamGraphReader);
};

// class AddGraphs
//
// GraphReader : concrete class StreamGraphReader
// FileWriter : concrete class BufferedWriter
template<class GraphReader, class FileWriter>
class AddGraphs {
 public:
  AddGraphs(GraphReader *g1, GraphReader *g2, FileWriter *writer)
  :graph1_(g1), graph2_(g2), writer_(writer) {
    assert(graph1_ != graph2_);
  }
  ~AddGraphs() { }
  void run() {
    NodeStream node1, node2;
    bool valid1 = false, valid2 = false;
    while (1) {
      if (!valid1 && graph1_->has_next()) {
        valid1 = true;
        graph1_->next_node(&node1);
      }
      if (!valid2 && graph2_->has_next()) {
        valid2 = true;
        graph2_->next_node(&node2);
      }
      if (PREDICT_TRUE(valid1 && valid2)) {
        // Both graphs have a node available
        if (node1.id == node2.id) {
          writer_->start_node(node1.id);
          writer_->add_edges(node1.list);
          valid1 = false;
          writer_->add_edges(node2.list);
          valid2 = false;
        } else if (node1.id < node2.id) {
          writer_->start_node(node1.id);
          writer_->add_edges(node1.list);
          valid1 = false;
        } else {
          writer_->start_node(node2.id);
          writer_->add_edges(node2.list);
          valid2 = false;
        }
        continue;
      } else if (valid1) {
        // Only graph 1 has nodes
        while (1) {
          writer_->start_node(node1.id);
          writer_->add_edges(node1.list);
          if (PREDICT_FALSE(!graph1_->has_next()))
            break;
          graph1_->next_node(&node2);
        }
      } else if (valid2) {
        // Only graph 2 has nodes
        while (1) {
          writer_->start_node(node2.id);
          writer_->add_edges(node2.list);
          if (PREDICT_FALSE(!graph2_->has_next()))
            break;
          graph2_->next_node(&node2);
        }
      }
      break;  // No more nodes available
    }
  }
 private:
  GraphReader *graph1_, *graph2_;
  FileWriter *writer_;
 private:
  DISALLOW_COPY_AND_ASSIGN(AddGraphs);
};

// class TransposeGraphPartially
//
// GraphReader : concrete class StreamGraphReader
// GraphWriter : concrete class GraphWriter
template<class GraphReader, class GraphWriter>
class TransposeGraphPartially {
  struct NodeList {
    node_t node;
    size_t next;
  };
 public:
  TransposeGraphPartially(GraphReader *graph, node_t start, node_t end,
      GraphWriter *writer )
    : graph_(graph), start_(start), end_(end), writer_(writer) {
    assert(start > 0);
    assert(end >= start);
    first_ = new int32_t[end_ - start_ + 1];
    // Initialize all nodes to have empty list
    memset(first_, -1, (end_ - start_ + 1)*sizeof(int32_t));
  }

  ~TransposeGraphPartially() {
    delete[] first_;
  }

  void run() {
    NodeStream node;
    while (graph_->has_next()) {
      graph_->next_node(&node);
      size_t len = node.list.size();
      for (size_t i = 0; i < len; i++) {
        node_t node_to = node.list[i];
        // Check if node_to is in desired range
        if (node_to < start_ || node_to > end_)
          continue;
        // Add edge to linked list
        link_list_.push_back((NodeList) {node.id, first_[node_to - start_]});
        first_[node_to - start_] = link_list_.size() - 1;
      }
    }
    // Write out the partial graph
    for (node_t node = start_; node <= end_; node++) {
      writer_->start_node(node);
      for (int i = first_[node - start_]; i != -1; i = link_list_[i].next) {
        writer_->add_edge(link_list_[i].node);
      }
    }
    // Release the memory from link_list_
    if (1) {
      vector<NodeList> tmp;
      swap(link_list_, tmp);
    }
  }
 private:
  GraphReader *graph_;
  node_t start_, end_;
  GraphWriter *writer_;
  vector<NodeList> link_list_;
  int32_t *first_;  // Start of list for each node
 private:
  DISALLOW_COPY_AND_ASSIGN(TransposeGraphPartially);
};

}  // namespace wikigraph

#endif  // SRC_GRAPH_H_

