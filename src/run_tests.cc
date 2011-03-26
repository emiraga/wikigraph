// Copyright 2011 Emir Habul, see file COPYING

#include <cstdio>

#include "writerlib.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
/* Example how to use writerlib.h */
/*
namespace wikigraph {
  void test_graph()
  {
    SystemFile f;
    if (!f.open("graph.txt", "w")) {
      perror("fopen");
      exit(1);
    }
    BufferedWriter<SystemFile> b(&f);
    GraphWriter<BufferedWriter<SystemFile> > g(&b, 2);
    g.start_node(2);
    g.add_edge(1);
  }
}  //namespace wikigraph
*/

int main(int argc, char *argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}

