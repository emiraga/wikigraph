// Copyright 2011 Emir Habul, see file COPYING

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "file_gzio.h"

using ::testing::_;
using ::testing::Gt;
using ::testing::Truly;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::InSequence;

namespace wikigraph {

TEST(GzipFile, simple) {
  GzipFile f;
  f.open("src/tests/0123456789.gz", "rb");
  char tmp[15];
  f.read(tmp, 1, 2);
  ASSERT_EQ('0', tmp[0]);
  ASSERT_EQ('1', tmp[1]);
  ASSERT_EQ(false, f.eof());
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
  ASSERT_EQ(false, f.eof());
  ASSERT_EQ(2, f.tell());

  f.seek(3, SEEK_SET);
  ASSERT_EQ(3, f.tell());

  f.read(tmp, 2, 1);
  ASSERT_EQ('3', tmp[0]);
  ASSERT_EQ('4', tmp[1]);
  ASSERT_EQ(false, f.eof());
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

