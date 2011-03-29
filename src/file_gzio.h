// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_FILE_GZIO_H_
#define SRC_FILE_GZIO_H_

#include <stdio.h>
#include <zlib.h>

#include "wikigraph_stubs_internal.h"

namespace wikigraph {

class GzipFile {
 public:
  GzipFile() { }
  bool open(const char *path, const char *mode) {
    FILE *f = fopen(path, mode);
    if (!f) return false;
    f_ = ::gzdopen(fileno(f), mode);
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
 private:
  gzFile f_;
  DISALLOW_COPY_AND_ASSIGN(GzipFile);
};

}  // namespace wikigraph

#endif  // SRC_FILE_GZIO_H_
