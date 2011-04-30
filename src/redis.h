// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_REDIS_H_
#define SRC_REDIS_H_

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "hiredis/hiredis.h"

namespace wikigraph {

// For some reason redisCommand does not return redisReply*
// Hiding this ugly syntax.
// TODO(user): should I make interface for redis?
redisReply* redisCmd(redisContext *c, const char *format, ...) {
  va_list argptr;
  va_start(argptr, format);
  redisReply* reply = reinterpret_cast<redisReply*>(
      redisvCommand(c, format, argptr));
  va_end(argptr);
  return reply;
}

namespace util {

vector<pii> count_items(vector<uint32_t> v) {
  sort(v.begin(), v.end());
  v.push_back(UINT32_MAX);

  vector<pii> result;

  int cnt = 0;
  for (size_t i = 0; i < v.size(); i++) {
    if (i && v[i-1] != v[i]) {
      result.push_back(pii(v[i-1], cnt));
      cnt = 1;
    } else {
      cnt++;
    }
  }
  return result;
}

string to_json(const vector<uint32_t> &v) {
  string msg = "[";
  for (size_t i = 0; i < v.size(); i++) {
    if (i) msg += ",";
    char msgpart[20];
    snprintf(msgpart, sizeof(msgpart), "%"PRIu32, v[i]);
    msg += string(msgpart);
  }
  msg += "]";
  return msg;
}

string to_json(const vector<pii> &v) {
  string msg = "[";
  for (size_t i = 0; i < v.size(); i++) {
    if (i) msg += ",";
    char msgpart[41];
    snprintf(msgpart, sizeof(msgpart), "[%"PRIu32",%"PRIu32"]",
        v[i].first, v[i].second);
    msg += string(msgpart);
  }
  msg += "]";
  return msg;
}

string to_json(const vector<pair<double, node_t> > &v) {
  string msg = "[";
  for (size_t i = 0; i < v.size(); i++) {
    if (i) msg += ",";
    char msgpart[41];
    snprintf(msgpart, sizeof(msgpart), "[%lf,%"PRIu32"]",
        v[i].first, v[i].second);
    msg += string(msgpart);
  }
  msg += "]";
  return msg;
}

}  // namespace util

}  // namespace wikigraph

#endif  // SRC_REDIS_H_

