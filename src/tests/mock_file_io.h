// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_TESTS_MOCK_FILE_IO_H_
#define SRC_TESTS_MOCK_FILE_IO_H_

#include "gmock/gmock.h"
#include "file_io.h"

namespace wikigraph {

class MockFile : public File {
 public:
  MOCK_METHOD2(open, bool(const char *path, const char *mode));
  MOCK_METHOD3(write, size_t(const void *ptr, size_t size, size_t nmemb));
  MOCK_METHOD3(read, size_t(void *ptr, size_t size, size_t nmemb));
  MOCK_METHOD0(close, int());
  MOCK_METHOD0(tell, off_t());
  MOCK_METHOD2(seek, int(off_t offset, int whence));
  MOCK_METHOD0(eof, int());
  MOCK_METHOD0(get_progress, double());
};

class MockFileWriter : public FileWriter {
 public:
  MOCK_METHOD0(finish, void());
  MOCK_METHOD1(write_uint, void(uint32_t val));
  MOCK_METHOD1(write_bit, void(bool val));
  MOCK_METHOD3(write, size_t(const void *ptr, size_t size, size_t nmemb));
};

template<class UnitType>
class MockFileReader : public FileReader<UnitType> {
 public:
  MOCK_CONST_METHOD0_T(eof, bool());
  MOCK_METHOD0_T(read_unit, UnitType());
  MOCK_METHOD0_T(peek_unit, UnitType());
  MOCK_METHOD2_T(read_from_back, void(UnitType *ptr, size_t nmemb));
};

class StubFile : public File {
 public:
  char *data_;
  off_t size_;
  off_t pos_;

  StubFile(void *data, size_t len)
  : data_(reinterpret_cast<char*>(data)), size_(len), pos_(0) { }

  bool open(const char *path, const char *mode) {
    assert(false);
  }
  size_t write(const void *ptr, size_t size, size_t nmemb) {
    assert(false);
  }
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
  int close() {
    data_ = NULL;
    return 0;
  }
  off_t tell() {
    return pos_;
  }
  int eof() {
    return pos_ >= size_;
  }
  double get_progress() {
    return 100 * pos_ / size_;
  }
};

}  // namespace wikigraph

#endif  // SRC_TESTS_MOCK_FILE_IO_H_

