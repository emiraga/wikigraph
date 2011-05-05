// Copyright 2011 Emir Habul, see file COPYING

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "tests/mock_file_io.h"

using ::testing::_;
using ::testing::Gt;
using ::testing::Truly;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::InSequence;

namespace wikigraph {

/* BufferedWriter */

TEST(BufferedWriter, empty) {
  MockFile s;
  BufferedWriter b(&s);
}

TEST(BufferedWriter, uint) {
  InSequence seq;
  MockFile s;
  EXPECT_CALL(s, write(_, 4, 1)).Times(1).WillOnce(Return(1));
  BufferedWriter b(&s);
  b.write_uint(45);
}

TEST(BufferedWriter, uint_buf2) {
  InSequence seq;
  MockFile s;
  EXPECT_CALL(s, write(_, 4, kBufferSize)).Times(2)
    .WillRepeatedly(Return(kBufferSize));
  BufferedWriter b(&s);
  for (uint32_t i = 0; i < 2*kBufferSize; i++)
    b.write_uint(i*1234);
}

TEST(BufferedWriter, write_bit) {
  InSequence seq;
  MockFile s;
  EXPECT_CALL(s, write(_, 4, 1)).Times(1).WillOnce(Return(1));
  BufferedWriter b(&s);
  b.write_bit(true);
  b.write_bit(false);
}

bool BW_write_bit2(const void *p) {
  const uint32_t *p2 = reinterpret_cast<const uint32_t*>(p);
  return p2[0] == (1u << 13);
}

TEST(BufferedWriter, write_bit2) {
  InSequence seq;
  MockFile s;
  EXPECT_CALL(s, write(Truly(BW_write_bit2), 4, 1))
    .Times(1).WillOnce(Return(1));
  BufferedWriter b(&s);
  for (int i = 0; i < 13; i++)
    b.write_bit(false);
  b.write_bit(true);
}

/* BufferedReader */

TEST(TestBufferedReader, empty) {
  MockFile s;
  BufferedReader<uint32_t> b(&s);
}

size_t ReadAction1(void *buff, size_t sz, size_t nmemb) {
  uint32_t *arg1 = reinterpret_cast<uint32_t*>(buff);
  arg1[0] = 23;
  arg1[1] = 24;
  arg1[2] = 25;
  return 3;
}

TEST(TestBufferedReader, read1) {
  InSequence seq;
  MockFile s;
  EXPECT_CALL(s, read(_, 4, Gt(3u))).Times(1).WillOnce(Invoke(ReadAction1));
  BufferedReader<uint32_t> b(&s);
  ASSERT_EQ(23u, b.peek_unit());
  ASSERT_EQ(23u, b.read_unit());
  ASSERT_EQ(24u, b.read_unit());
  ASSERT_EQ(25u, b.peek_unit());
  ASSERT_EQ(25u, b.read_unit());
}

size_t ReadAction2(void *buff, size_t size, size_t nmemb) {
  uint32_t *arg1 = reinterpret_cast<uint32_t*>(buff);
  for (size_t i = 0; i < nmemb; i++) {
    arg1[i] = i;
  }
  return nmemb;
}

TEST(TestBufferedReader, read2) {
  InSequence seq;
  MockFile s;
  EXPECT_CALL(s, read(_, 4, kBufferSize))
    .Times(2).WillRepeatedly(Invoke(ReadAction2));

  BufferedReader<uint32_t> b(&s);
  for (size_t i = 0; i < 2*kBufferSize; i++) {
    ASSERT_EQ(i % kBufferSize, b.peek_unit() );
    ASSERT_EQ(i % kBufferSize, b.read_unit() );
  }
}

TEST(TestBufferedReader, using_stub) {
  uint32_t data[5] = {1, 2, 3, 4, 5};
  StubFile fs(data, sizeof(data));
  BufferedReader<uint32_t> b(&fs);
  ASSERT_EQ(1u, b.peek_unit());
  uint32_t tmp[2];
  b.read_from_back(tmp, 2);
  ASSERT_EQ(tmp[0], 4u);
  ASSERT_EQ(tmp[1], 5u);
  ASSERT_EQ(1u, b.read_unit());
  ASSERT_EQ(2u, b.read_unit());
  b.read_from_back(tmp, 1);
  ASSERT_EQ(tmp[0], 5u);
  ASSERT_EQ(3u, b.read_unit());
}

TEST(GzipFile, simple) {
  GzipFile f;
  f.open("src/tests/0123456789.gz", "rb");
  char tmp[15];
  f.read(tmp, 1, 2);
  ASSERT_EQ('0', tmp[0]);
  ASSERT_EQ('1', tmp[1]);
  ASSERT_FALSE(f.eof());
  memset(tmp, 0, 15);
  f.read(tmp, 1, 10);
  ASSERT_STREQ("23456789", tmp);
  ASSERT_EQ(true, f.eof());
  f.close();
}

TEST(GzipFile, seek_tell) {
  GzipFile f;
  f.open("src/tests/0123456789.gz", "rb");
  char tmp[15];
  f.read(tmp, 2, 1);
  ASSERT_EQ('0', tmp[0]);
  ASSERT_EQ('1', tmp[1]);
  ASSERT_FALSE(f.eof());
  ASSERT_EQ(2, f.tell());

  f.seek(3, SEEK_SET);
  ASSERT_EQ(3, f.tell());

  f.read(tmp, 2, 1);
  ASSERT_EQ('3', tmp[0]);
  ASSERT_EQ('4', tmp[1]);
  ASSERT_FALSE(f.eof());
  ASSERT_EQ(5, f.tell());

  f.seek(-3, SEEK_CUR);
  f.read(tmp, 1, 1);
  ASSERT_EQ('2', tmp[0]);

  // SEEK_END is not supported wtf?
  //
  // f.seek(-2, SEEK_END);
  // f.read(tmp, 2, 1);
  // ASSERT_EQ('8', tmp[0]);
  // ASSERT_EQ('9', tmp[0]);
  f.close();
}

}  // namespace wikigraph

