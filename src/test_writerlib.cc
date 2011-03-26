// Copyright 2011 Emir Habul, see file COPYING

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "mock_writerlib.h"

using ::testing::_;
using ::testing::Truly;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::InSequence;

namespace wikigraph {

TEST(TestBufferedWriter, empty) {
  MockSystemFile s;
  EXPECT_CALL(s, close()).Times(AtLeast(1));
  BufferedWriter<MockSystemFile> b(&s);
}

TEST(TestBufferedWriter, uint) {
  InSequence seq;
  MockSystemFile s;
  EXPECT_CALL(s, write(_, 4, 1)).Times(1).WillOnce(Return(1));
  EXPECT_CALL(s, close()).Times(AtLeast(1));
  BufferedWriter<MockSystemFile> b(&s);
  b.write_uint(45);
}

TEST(TestBufferedWriter, uint_buf2) {
  InSequence seq;
  MockSystemFile s;
  EXPECT_CALL(s, write(_, 4, kBufferSize)).Times(2)
    .WillRepeatedly(Return(kBufferSize));
  EXPECT_CALL(s, close()).Times(AtLeast(1));
  BufferedWriter<MockSystemFile> b(&s);
  for (uint32_t i = 0; i < 2*kBufferSize; i++)
    b.write_uint(i*1234);
}

bool GraphBasicMatch(const void *p) {
  const node_t *arg = reinterpret_cast<const node_t *>(p);
  return
    arg[3] - arg[2] == 1 &&  // Check node 1
    arg[5] - arg[4] == 3 &&  // Check node 2
    arg[7] - arg[6] == 0;    // Check node 3
}

TEST(GraphWriter, basic) {
  InSequence seq;
  MockBufferedWriter b;

  EXPECT_CALL(b, write_uint(1)).Times(1);  // edge for node 1
  EXPECT_CALL(b, write_uint(2)).Times(1);  // edge
  EXPECT_CALL(b, write_uint(3)).Times(1);  // edge
  EXPECT_CALL(b, write_uint(2)).Times(1);  // edge for node 2
  EXPECT_CALL(b, write(Truly(GraphBasicMatch), 4, 8))
    .Times(1);                             // node list 3 nodes
  EXPECT_CALL(b, write_uint(4)).Times(1);  // num edges
  EXPECT_CALL(b, write_uint(3)).Times(1);  // num nodes
  EXPECT_CALL(b, close()).Times(AtLeast(1));

  GraphWriter<MockBufferedWriter> g(&b, 3);
  g.start_node(2);
  g.add_edge(1);
  g.add_edge(2);
  g.add_edge(3);
  g.start_node(1);
  g.add_edge(2);
}

}  // namespace wikigraph

