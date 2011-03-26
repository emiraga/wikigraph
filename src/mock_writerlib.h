// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_MOCK_WRITERLIB_H_
#define SRC_MOCK_WRITERLIB_H_

#include "writerlib.h"
#include "gmock/gmock.h"

namespace wikigraph {

class MockSystemFile {
 public:
  MOCK_METHOD2(open,  bool(const char *path, const char *mode));
  MOCK_METHOD3(write, size_t(const void *ptr, size_t size, size_t nmemb));
  MOCK_METHOD3(read,  size_t(void *ptr, size_t size, size_t nmemb));
  MOCK_METHOD0(close, int());
};

class MockBufferedWriter {
 public:
  MOCK_METHOD0(flush, void());
  MOCK_METHOD0(close, void());
  MOCK_METHOD1(write_uint, void(uint32_t val));
  MOCK_METHOD1(write_bit, void(bool val));
  MOCK_METHOD3(write, size_t(const void *ptr, size_t size, size_t nmemb));
};

class MockGraphWriter {
 public:
  MOCK_METHOD1(start_node, void(node_t node));
  MOCK_METHOD1(add_edge, void(node_t edge));
  MOCK_METHOD0(close, void());
};

}  // namespace wikigraph

#endif  // SRC_MOCK_WRITERLIB_H_

