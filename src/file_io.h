// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_FILE_IO_H_
#define SRC_FILE_IO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <numeric>

#include "wikigraph_stubs_internal.h"

namespace wikigraph {

const unsigned int kBufferSize = 1024*1024;  // for buffered reader and writer

class File {
 public:
  virtual ~File() { }
  virtual bool open(const char *path, const char *mode) = 0;
  virtual size_t write(const void *ptr, size_t size, size_t nmemb) = 0;
  virtual size_t read(void *ptr, size_t size, size_t nmemb) = 0;
  virtual int close() = 0;
  virtual off_t tell() = 0;
  virtual int seek(off_t offset, int whence) = 0;
  virtual int eof() = 0;
  // Only useful when reading files
  virtual double get_progress() = 0;
};

class SystemFile : public File {
 public:
  SystemFile() { }
  bool open(const char *path, const char *mode) {
    file_size_ = 0;
    f_ = ::fopen(path, mode);
    return f_ != NULL;
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
    if(!file_size_)
      file_size_ = file_size(); 
    return 100.0 * tell() / file_size_;
  }
 private:
  off_t file_size() {
    off_t pos = tell();
    seek(0, SEEK_END);
    off_t ret = tell();
    seek(pos, SEEK_SET);
    return ret;
  }
  FILE *f_;
  off_t file_size_;
  DISALLOW_COPY_AND_ASSIGN(SystemFile);
};

class GzipFile : public File {
 public:
  GzipFile() { }
  bool open(const char *path, const char *mode) {
    file_size_ = 0;
    f_plain_ = fopen(path, mode);
    if (!f_plain_)
      return false;
    // Open az gzip
    f_ = ::gzdopen(fileno(f_plain_), mode);
    return f_ != Z_NULL;
  }
  size_t write(const void *ptr, size_t size, size_t nmemb) {
    assert(false);  // not interested in writing gz files
    // return ::gzwrite(f_, ptr, size * nmemb);
  }
  size_t read(void *ptr, size_t size, size_t nmemb) {
    return ::gzread(f_, ptr, size * nmemb);
  }
  int close() {
    return ::gzclose(f_);
  }
  off_t tell() {
    return ::gztell(f_);
  }
  int seek(off_t offset, int whence) {
    assert(whence != SEEK_END);  // per gzseek man page
    return ::gzseek(f_, offset, whence);
  }
  int eof() {
    return ::gzeof(f_);
  }
  // Only useful when reading files
  double get_progress() {
    if(!file_size_)
      file_size_ = file_size();
    // To obtain current position in a file, we query file pointer
    // of the compressed file. There are many problems with this:
    // mainly, is that it is not very precise, and potentially not portable.
    return 100.0 * lseek(fileno(f_plain_), 0, SEEK_CUR) / file_size_;
  }
 private:
  off_t file_size() {
    // Determine file size (used in get_progress)
    // Potentially not portable!
    struct stat file_info;
    fstat(fileno(f_plain_), &file_info);
    return file_info.st_size;
  }
  gzFile f_;
  FILE *f_plain_;
  off_t file_size_;
  DISALLOW_COPY_AND_ASSIGN(GzipFile);
};

class FileWriter {
 public:
  virtual ~FileWriter() { }
  virtual void finish() = 0;
  virtual void write_uint(uint32_t val) = 0;
  virtual void write_bit(bool val) = 0;
  virtual size_t write(const void *ptr, size_t size, size_t nmemb) = 0;
};

class BufferedWriter : public FileWriter {
 public:
  explicit BufferedWriter(File *f)
      :file_(f), pos_(0), bitpos_(0) {
    buffer_ = new uint32_t[kBufferSize];
  }

  ~BufferedWriter() {
    finish();
    delete[] buffer_;
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
  uint32_t *buffer_;  // [kBufferSize];
  File *file_;
  size_t pos_;
  int bitpos_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedWriter);
};

template<class UnitType>
class FileReader {
 public:
  virtual ~FileReader() { }
  virtual bool eof() const = 0;
  virtual UnitType read_unit() = 0;
  virtual UnitType peek_unit() = 0;
  virtual void read_from_back(UnitType *ptr, size_t nmemb) = 0;
  virtual void set_print_progress(bool do_print) = 0;
};

template<class UnitType>
class BufferedReader : public FileReader<UnitType> {
 public:
  explicit BufferedReader(File *f)
      :file_(f), read_size_(0), index_(0), print_progress_(false) {
    buffer_ = new UnitType[kBufferSize];
  }

  ~BufferedReader() {
    delete[] buffer_;
  }

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

  void set_print_progress(bool do_print) {
    print_progress_ = do_print;
  }
 private:
  void read_buffer() {
    if(print_progress_) {
      printf(" %6.3lf%%\n", file_->get_progress());
    }
    read_size_ = file_->read(buffer_, sizeof(UnitType), kBufferSize);
  }

  UnitType *buffer_;  // [kBufferSize];
  File *file_;
  size_t read_size_;
  size_t index_;
  bool print_progress_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedReader);
};

}  // namespace wikigraph

#endif  // SRC_FILE_IO_H_

