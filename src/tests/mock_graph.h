// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_TESTS_MOCK_GRAPH_H_
#define SRC_TESTS_MOCK_GRAPH_H_

#include "gmock/gmock.h"
#include "wikigraph_stubs_internal.h"

namespace wikigraph {

class MockGraphWriter {
 public:
  MOCK_METHOD1(start_node, void(node_t node));
  MOCK_METHOD1(add_edge, void(node_t edge));
  MOCK_METHOD1(add_edges, void(const vector<node_t> &edges));
  MOCK_METHOD0(close, void());
};

}  // namespace wikigraph

#endif  // SRC_TESTS_MOCK_GRAPH_H_
