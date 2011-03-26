// Copyright 2011 Emir Habul, see file COPYING

#include <cstdio>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "graph.h"
#include "tests/mock_writerlib.h"

using ::testing::_;
using ::testing::Truly;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::InSequence;

namespace wikigraph {

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

TEST(StreamGraphReader, using_fs_stub) {
  uint32_t data[13] = {
    1, 3,
    1,
    1000, 1000,  // n0
    1000, 1000,  // n1
    0, 2,  // n2
    2, 3,  // n3
    3, 3,
  };
  StubFileSystem fs(data, 13);
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

}  // namespace wikigraph

