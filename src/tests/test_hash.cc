// Copyright 2011 Emir Habul, see file COPYING

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "hash.h"

using ::testing::_;
using ::testing::Gt;
using ::testing::Truly;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::InSequence;

namespace wikigraph {

TEST(ComputeHash, simple) {
  ComputeHash c;
  c.ProcessString("test", 4);
  ASSERT_STREQ("098f6bcd4621", c.get_hash());
}

}  // namespace wikigraph

