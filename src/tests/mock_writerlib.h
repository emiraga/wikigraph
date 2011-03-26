// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_TESTS_MOCK_WRITERLIB_H_
#define SRC_TESTS_MOCK_WRITERLIB_H_

#include "writerlib.h"
#include "gmock/gmock.h"

namespace wikigraph {

class MockSystemFile {
 public:
  MOCK_METHOD2(open,  bool(const char *path, const char *mode));
  MOCK_METHOD3(write, size_t(const void *ptr, size_t size, size_t nmemb));
  MOCK_METHOD3(read,  size_t(void *ptr, size_t size, size_t nmemb));
  MOCK_METHOD0(close, int());
  MOCK_METHOD0(eof, int());
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

class StubFileSystem {
 public:
  uint32_t *data_;
  off_t size_;
  off_t pos_;

  StubFileSystem(uint32_t *data, size_t len)
  : data_(data), size_(len), pos_(0) { }

  int seek(off_t off, int whence) {
    assert(((off%4)+4)%4 == 0);
    off /= 4;

    if (whence == SEEK_CUR) pos_ += off;
    else if (whence == SEEK_SET) pos_ = off;
    else if (whence == SEEK_END) pos_ = size_ + off;
    if (pos_ < 0) pos_ = 0;
    if (pos_ > size_) pos_ = size_;
    return 0;
  }
  size_t read(uint32_t *ptr, unsigned int elemsize, size_t nmemb) {
    assert(elemsize == 4);
    size_t ret;
    for (ret = 0; ret < nmemb; ret++) {
      ptr[ret] = data_[pos_];
      pos_++;
      if (pos_ == size_)
        break;
    }
    return ret;
  }
  void close() {
    data_ = NULL;
  }
  size_t tell() {
    return pos_ * 4;
  }
};

}  // namespace wikigraph

#endif  // SRC_TESTS_MOCK_WRITERLIB_H_

