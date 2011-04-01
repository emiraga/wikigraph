// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_FILE_GZIO_H_
#define SRC_FILE_GZIO_H_

#include <stdio.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wikigraph_stubs_internal.h"

namespace wikigraph {

class GzipFile {
 public:
  GzipFile() { }
  bool open(const char *path, const char *mode) {
    f_plain_ = fopen(path, mode);
    if (!f_plain_)
      return false;
    // Determine file size (used in get_progress)
    struct stat file_info;
    fstat(fileno(f_plain_), &file_info);
    file_size_ = file_info.st_size;
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
    // To obtain current position in a file, we query file pointer
    // of the compressed file. There are many problems with this:
    // mainly, is that it is not very precise, and potentially not portable.
    return 100.0 * lseek(fileno(f_plain_), 0, SEEK_CUR) / file_size_;
  }
 private:
  gzFile f_;
  FILE *f_plain_;
  off_t file_size_;
  DISALLOW_COPY_AND_ASSIGN(GzipFile);
};

}  // namespace wikigraph

#endif  // SRC_FILE_GZIO_H_

