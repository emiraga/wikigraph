// Copyright 2011 Emir Habul, see file COPYING

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "graph.h"

namespace wikigraph {

int print_graph(char *fname) {
  SystemFile f;
  if (!f.open(fname, "rb")) {
    fprintf(stderr, "Could not open %s\n", fname);
    return 1;
  }
  BufferedReader<uint32_t> reader(&f);
  StreamGraphReader graph(&reader);
  graph.init();
  printf("Nodes: %"PRIu32"\n", graph.get_num_nodes());
  printf("Edges: %"PRIu32"\n", graph.get_num_edges());
  NodeStream node;
  while (graph.has_next()) {
    graph.next_node(&node);
    printf("%"PRIu32": ", node.id);
    for (size_t i = 0; i < node.list.size(); i++) {
      printf(" %"PRIu32, node.list[i]);
    }
    printf("\n");
  }
  return 0;
}

}  // namespace wikigraph

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s file.graph\n", argv[0]);
    return 1;
  }
  return wikigraph::print_graph(argv[1]);
}

