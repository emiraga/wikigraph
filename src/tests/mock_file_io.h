// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_TESTS_MOCK_FILE_IO_H_
#define SRC_TESTS_MOCK_FILE_IO_H_

#include "file_io.h"
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

class StubFileSystem {
 public:
  char *data_;
  off_t size_;
  off_t pos_;

  StubFileSystem(void *data, size_t len)
  : data_(reinterpret_cast<char*>(data)), size_(len), pos_(0) { }

  int seek(off_t off, int whence) {
    if (whence == SEEK_CUR) pos_ += off;
    else if (whence == SEEK_SET) pos_ = off;
    else if (whence == SEEK_END) pos_ = size_ + off;
    return 0;
  }
  size_t read(void *vptr, unsigned int elemsize, size_t nmemb) {
    if (eof())
      return 0;
    char *ptr = reinterpret_cast<char*>(vptr);
    nmemb *= elemsize;

    size_t ret;
    for (ret = 0; ret < nmemb; ret++) {
      ptr[ret] = data_[pos_];
      pos_++;
      if (eof())
        break;
    }
    return ret;
  }
  void close() {
    data_ = NULL;
  }
  size_t tell() {
    return pos_;
  }
  bool eof() const {
    return pos_ >= size_;
  }
};

}  // namespace wikigraph

#endif  // SRC_TESTS_MOCK_FILE_IO_H_

