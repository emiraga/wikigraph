// Copyright 2011 Emir Habul, see file COPYING

#include <cstdio>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "graph.h"
#include "tests/mock_file_io.h"
#include "tests/mock_graph.h"

using ::testing::_;
using ::testing::Truly;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::InSequence;

namespace wikigraph {

typedef StreamGraphReader<BufferedReader<StubFileSystem,
  uint32_t> > StubGraphReader;

TEST(BitArray, basic) {
  const int s = 10;
  BitArray b(s);
  for (int i = 0; i < s; i++) ASSERT_FALSE(b.get_value(i));
  b.set_true(5);
  for (int i = 0; i < s; i++) ASSERT_EQ(i == 5, b.get_value(i));
  b.set_false(5);
  for (int i = 0; i < s; i++) ASSERT_FALSE(b.get_value(i));
}

TEST(BitArray, MultiDWordEachThird) {
  const int s = 200;
  BitArray b(s);
  for (int i = 0; i < s; i+=3) b.set_true(i);
  for (int i = 0; i < s; i++) ASSERT_EQ(i%3 == 0, b.get_value(i));
  for (int i = 0; i < s; i+=3) b.set_false(i);
  for (int i = 0; i < s; i++) ASSERT_FALSE(b.get_value(i));
}

TEST(BitArray, MultiDWordAll) {
  const int s = 200;
  BitArray b(s);
  for (int i = 0; i < s; i++) ASSERT_FALSE(b.get_value(i));
  for (int i = 0; i < s; i++) b.set_true(i);
  for (int i = 0; i < s; i++) ASSERT_TRUE(b.get_value(i));
  for (int i = 0; i < s; i++) b.set_false(i);
  for (int i = 0; i < s; i++) ASSERT_FALSE(b.get_value(i));
}

/* test StreamGraphReader */

TEST(StreamGraphReader, using_fs_stub) {
  uint32_t data[10] = {
    1, 3,
    1,
    0,  // n0
    0,  // n1
    0,  // n2
    2,  // n3
    3,  // extra
    3, 3,
  };
  StubFileSystem fs(data, sizeof(data));
  BufferedReader<StubFileSystem, uint32_t> b(&fs);
  NodeStream node;
  StreamGraphReader<BufferedReader<StubFileSystem, uint32_t> > g(&b);
  g.init();

  ASSERT_TRUE(g.has_next());
  g.next_node(&node);
  ASSERT_EQ(2u, node.id);
  ASSERT_EQ(2u, node.list.size());
  ASSERT_EQ(1u, node.list[0]);
  ASSERT_EQ(3u, node.list[1]);

  ASSERT_TRUE(g.has_next());
  g.next_node(&node);
  ASSERT_EQ(3u, node.id);
  ASSERT_EQ(1u, node.list.size());
  ASSERT_EQ(1u, node.list[0]);

  ASSERT_FALSE(g.has_next());
}

/* GraphWriter */

bool GraphBasicMatch(const void *p) {
  const node_t *arg = reinterpret_cast<const node_t *>(p);
  // for(int i = 0; i < 5; i++) {
  //   printf("arg[%d]=%d\n", i, arg[i]);
  // }
  return
    arg[2] - arg[1] == 1 &&  // Check node 1
    arg[3] - arg[2] == 3 &&  // Check node 2
    arg[4] - arg[3] == 0;    // Check node 3
}

TEST(GraphWriter, basic) {
  InSequence seq;
  MockBufferedWriter b;

  EXPECT_CALL(b, write_uint(2)).Times(1);  // edge for node 1
  EXPECT_CALL(b, write_uint(1)).Times(1);  // edge for node 2
  EXPECT_CALL(b, write_uint(2)).Times(1);  // edge
  EXPECT_CALL(b, write_uint(3)).Times(1);  // edge
  EXPECT_CALL(b, write(Truly(GraphBasicMatch), 4, 3+2))
    .Times(1);                             // node list 3 nodes
  EXPECT_CALL(b, write_uint(4)).Times(1);  // num edges
  EXPECT_CALL(b, write_uint(3)).Times(1);  // num nodes
  // EXPECT_CALL(b, close()).Times(AtLeast(1));

  GraphWriter<MockBufferedWriter> g(&b, 3);
  g.start_node(1);
  g.add_edge(2);
  g.start_node(2);
  g.add_edge(1);
  g.add_edge(2);
  g.add_edge(3);
}

bool GraphWithHoleMatch(const void *p) {
  const node_t *arg = reinterpret_cast<const node_t *>(p);
  // for(int i = 0; i < 9; i++) {
  //   printf("arg[%d]=%d\n", i, arg[i]);
  // }
  return arg[0] == 0
    && arg[1] == 0
    && arg[2] == 0
    && arg[3] == 1
    && arg[4] == 1
    && arg[5] == 1
    && arg[6] == 2
    && arg[7] == 2
    && arg[8] == 2
    && arg[9] == 2;
}

TEST(GraphWriter, WithHole) {
  InSequence seq;
  MockBufferedWriter b;

  EXPECT_CALL(b, write_uint(1)).Times(1);  // edge for node 2
  EXPECT_CALL(b, write_uint(3)).Times(1);  // edge for node 5
  EXPECT_CALL(b, write(Truly(GraphWithHoleMatch), 4, 8+2))
    .Times(1);                             // node list 5 nodes
  EXPECT_CALL(b, write_uint(2)).Times(1);  // num edges
  EXPECT_CALL(b, write_uint(8)).Times(1);  // num nodes
  // EXPECT_CALL(b, close()).Times(AtLeast(1));

  GraphWriter<MockBufferedWriter> g(&b, 8);
  g.start_node(2);
  g.add_edge(1);
  g.start_node(5);
  g.add_edge(3);
}

/* AddGraphs */

TEST(AddGraphs, using_fs_stub) {
  uint32_t data[10] = {
    1, 3,
    1,
    0,  // n0
    0,  // n1
    0,  // n2
    2,  // n3
    3,  // extra
    3, 3,
  };
  StubFileSystem fs1(data, sizeof(data));
  StubFileSystem fs2(data, sizeof(data));
  BufferedReader<StubFileSystem, uint32_t> b1(&fs1);
  BufferedReader<StubFileSystem, uint32_t> b2(&fs2);

  StubGraphReader g1(&b1);
  StubGraphReader g2(&b2);

  MockBufferedWriter wr;
  GraphWriter<MockBufferedWriter> grwr(&wr, 3);

  InSequence seq;
  EXPECT_CALL(wr, write_uint(1)).Times(1);  // Node 2
  EXPECT_CALL(wr, write_uint(3)).Times(1);
  EXPECT_CALL(wr, write_uint(1)).Times(1);
  EXPECT_CALL(wr, write_uint(3)).Times(1);

  EXPECT_CALL(wr, write_uint(1)).Times(1);  // Node 3
  EXPECT_CALL(wr, write_uint(1)).Times(1);

  EXPECT_CALL(wr, write(_, 4, 3+2)).Times(1);

  EXPECT_CALL(wr, write_uint(6)).Times(1);  // edges
  EXPECT_CALL(wr, write_uint(3)).Times(1);  // nodes
  // EXPECT_CALL(wr, close()).Times(1);
  AddGraphs<StubGraphReader, GraphWriter<MockBufferedWriter> >
    add(&g1, &g2, &grwr);
  g1.init();
  g2.init();
  add.run();
}

/* TransposeGraphPartially */

TEST(TransposeGraphPartially, simple1) {
  uint32_t data[10] = {
    1, 3,
    1,
    0,  // n0
    0,  // n1
    0,  // n2
    2,  // n3
    3,  // extra
    3, 3,
  };
  StubFileSystem fs(data, sizeof(data));
  BufferedReader<StubFileSystem, uint32_t> b(&fs);
  StubGraphReader g(&b);
  g.init();

  InSequence seq;
  MockGraphWriter grwr;
  EXPECT_CALL(grwr, start_node(3)).Times(1);  // Node 3
  EXPECT_CALL(grwr, add_edge(2)).Times(1);

  TransposeGraphPartially<StubGraphReader, MockGraphWriter>
    trans(&g, 2, 3, &grwr);
  trans.run();
}

TEST(TransposeGraphPartially, simple2) {
  uint32_t data[10] = {
    1, 3,
    1,
    0,  // n0
    0,  // n1
    0,  // n2
    2,  // n3
    3,  // extra
    3, 3,
  };
  StubFileSystem fs(data, sizeof(data));
  BufferedReader<StubFileSystem, uint32_t> b(&fs);
  StubGraphReader g(&b);
  g.init();

  // Not in sequence
  MockGraphWriter grwr;
  EXPECT_CALL(grwr, start_node(1)).Times(1);  // Node 1
  EXPECT_CALL(grwr, add_edge(3)).Times(1);
  EXPECT_CALL(grwr, add_edge(2)).Times(1);

  TransposeGraphPartially<StubGraphReader, MockGraphWriter>
    trans(&g, 1, 1, &grwr);
  trans.run();
}

}  // namespace wikigraph

