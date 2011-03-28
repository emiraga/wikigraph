// Copyright 2011 Emir Habul, see file COPYING

#include "gtest/gtest.h"
#include "gmock/gmock.h"

int main(int argc, char *argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}

