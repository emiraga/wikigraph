// Copyright 2011 Emir Habul, see file COPYING

#include "gtest/gtest.h"
#include "gmock/gmock.h"

int main(int argc, char *argv[]) {
#ifdef NDEBUG
  fprintf(stderr, "Warning: assertions are turned off.");
  assert(false);  // You see, nothing happens.
#endif
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}

