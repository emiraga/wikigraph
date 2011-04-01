// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_FILE_IO_H_
#define SRC_FILE_IO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <numeric>

#include "wikigraph_stubs_internal.h"

namespace wikigraph {

const unsigned int kBufferSize = 1024*1024;  // for buffered reader and writer

class SystemFile {
 public:
  SystemFile() { }
  bool open(const char *path, const char *mode) {
    f_ = ::fopen(path, mode);
    if (!f_)
      return false;
    // Determine file size (used in get_progress)
    ::fseeko(f_, 0, SEEK_END);
    file_size_ = ::ftello(f_);
    ::fseeko(f_, 0, SEEK_SET);
    return true;
  }
  size_t write(const void *ptr, size_t size, size_t nmemb) {
    return ::fwrite(ptr, size, nmemb, f_);
  }
  size_t read(void *ptr, size_t size, size_t nmemb) {
    return ::fread(ptr, size, nmemb, f_);
  }
  int close() {
    return ::fclose(f_);
  }
  off_t tell() {
    return ::ftello(f_);
  }
  int seek(off_t offset, int whence) {
    return ::fseeko(f_, offset, whence);
  }
  int eof() {
    return ::feof(f_);
  }
  // Only useful when reading files
  double get_progress() {
    return 100.0 * tell() / file_size_;
  }
 private:
  FILE *f_;
  off_t file_size_;
  DISALLOW_COPY_AND_ASSIGN(SystemFile);
};

// Write data to the file
template<class File>
class BufferedWriter {
 public:
  explicit BufferedWriter(File *f)
      :file_(f), pos_(0), bitpos_(0) { }

  ~BufferedWriter() {
    finish();
  }

  void flush() {
    size_t res = file_->write(buffer_, sizeof(uint32_t), pos_);
    assert(res == pos_);
    pos_ = 0;
  }

  void finish() {
    if (file_ == NULL)
      return;

    // Flush written bits
    if (bitpos_) {
      pos_++;
      // Extra bits will be filled with zeroes
      bitpos_ = 0;
    }
    if (pos_)
      flush();  // buffers
    file_ = NULL;
  }

  void write_uint(uint32_t val) {
    assert(bitpos_ == 0);  // Don't mix bits with uints

    buffer_[pos_++] = val;
    if (PREDICT_FALSE(pos_ == kBufferSize))
      flush();
  }

  void write_bit(bool val) {
    if (PREDICT_FALSE(bitpos_ == 0))
      buffer_[pos_] = val;
    else
      buffer_[pos_] |= (val << bitpos_);

    bitpos_++;
    if (PREDICT_FALSE(bitpos_ == 32)) {
      bitpos_ = 0;
      pos_++;
      if (PREDICT_FALSE(pos_ == kBufferSize))
        flush();
    }
  }
  size_t write(const void *ptr, size_t size, size_t nmemb) {
    flush();  // Make sure we remove any bits or uints
    return file_->write(ptr, size, nmemb);
  }
 private:
  uint32_t buffer_[kBufferSize];
  File *file_;
  size_t pos_;
  int bitpos_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedWriter);
};

// Read data in units from file
template<class File, class UnitType>
class BufferedReader {
 public:
  explicit BufferedReader(File *f)
      :file_(f), read_size_(0), index_(0) { }

  ~BufferedReader() { }

  bool eof() const {
    return read_size_ == 0 && file_->eof();
  }

  UnitType read_unit() {
    index_++;
    if (PREDICT_FALSE(index_ >= read_size_)) {
      index_ = 0;
      read_buffer();
    }
    return buffer_[index_];
  }

  UnitType peek_unit() {
    if (PREDICT_FALSE(index_+1 >= read_size_)) {
      index_ = -1;
      read_buffer();
    }
    return buffer_[index_+1];
  }

  // Read from back is not buffered, and should not be done very often
  void read_from_back(UnitType *ptr, size_t nmemb) {
    // calculate current position
    off_t pos = file_->tell();

    // Read from back
    file_->seek(-off_t(sizeof(UnitType)*nmemb), SEEK_END);
    file_->read(ptr, sizeof(UnitType), nmemb);

    // Return back to original position
    file_->seek(pos, SEEK_SET);
  }

 private:
  void read_buffer() {
    read_size_ = file_->read(buffer_, sizeof(UnitType), kBufferSize);
  }

  UnitType buffer_[kBufferSize];
  File *file_;
  size_t read_size_;
  size_t index_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedReader);
};

}  // namespace wikigraph

#endif  // SRC_FILE_IO_H_

