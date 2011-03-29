// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_HASH_H_
#define SRC_HASH_H_

#include <stddef.h>
#include <stdint.h>
#include "md5/md5.h"
#include "wikigraph_stubs_internal.h"

namespace wikigraph {

namespace internal {
const char hexchars[16] = {
  '0', '1', '2', '3', '4', '5', '6',
  '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
}  // internal

class ComputeHash {
 public:
  static const int kHashLen = 6;
  ComputeHash() {
    hash_[kHashLen * 2] = 0;  // 0-terminated
  }
  void ProcessString(const char *str, int len) {
    md5_state_t state;
    md5_byte_t digest[16];

    md5_init(&state);
    md5_append(&state, (const md5_byte_t *)str, len);
    md5_finish(&state, digest);

    for (int di = 0; di < kHashLen; di++) {
      hash_[di * 2] = internal::hexchars[digest[di]>>4];
      hash_[di*2+1] = internal::hexchars[digest[di]&15];
    }
  }
  const char* get_hash() const {
    return hash_;
  }
 private:
  char hash_[kHashLen * 2 + 1];
 private:
  DISALLOW_COPY_AND_ASSIGN(ComputeHash);
};

}  // namespace wikigraph

#endif  // SRC_HASH_H_

