// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_SQL_PARSER_H_
#define SRC_SQL_PARSER_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <cctype>
#include <vector>

#include "wikigraph_stubs_internal.h"
#include "file_io.h"

namespace wikigraph {

class DataHandler {
 public:
  virtual ~DataHandler() { }
  virtual void data(const vector<string> &row) = 0;
};

class SqlParser {
 public:
  SqlParser(FileReader<char> *file, DataHandler *data_handler)
  : f_(file), handler_(data_handler) { }
  ~SqlParser() { }
  bool eof() const {
    return f_->eof();
  }
  void run() {
    while (1) {
      char c = get();
      if (f_->eof())
        break;
      if (isspace(c) || c == ';')
        continue;
      if (c == '-' and peek() == '-')  // --comment
        skip_after('\n');
      else if (c == '/' and peek() == '*') {
        get();
        skip_after('*', '/');  /* comment */
      } else if (isalpha(c)) {
        if (toupper(c) == 'I') {
          insert_command();
        } else {
          skip_command();  // don't care
        }
      } else {
        assert(false);
        // printf("Wrong beginning '%c' (%d)\n", c, c);
        return;
      }
    }
  }
 private:
  char get() {
    return f_->read_unit();
  }
  char peek() {
    return f_->peek_unit();
  }
  void insert_command() {
    while (peek() != '(')
      get();
    while (1) {
      data_brackets();
      char c = get();
      if (c == ',')
        continue;
      if (c == ';')
        break;
      // printf("Unexpected: '%c'\n", c);
      assert(false);
    }
  }
  void data_brackets() {
    assert(peek() == '(');
    get();
    vector<string> data;
    while (1) {
      char c = peek();
      if (c == '\'')
        data.push_back(get_quoted_string());
      else if (isdigit(c) || c == '-')
        data.push_back(get_number());
      else if (c == 'N') {
        get();
        assert(peek() == 'U');
        get();
        assert(peek() == 'L');
        get();
        assert(peek() == 'L');
        get();
      } else {
        // printf("Unexpected: '%c'\n", c);
        assert(false);
      }
      c = get();
      if (c == ',')
        continue;
      if (c == ')')
        break;
      // printf("Unexpected: '%c'\n", c);
      assert(false);
    }
    handler_->data(data);
  }
  void skip_command() {
    while (1) {
      char c = peek();
      if (c == ';') {
        get();
        break;
      }
      if (c == '-') {
        get();
        if (peek() == '-') {
          skip_after('\n');
        }
      } else if (c == '/') {
        get();
        if (peek() == '*') {
          get();
          skip_after('*', '/');
        }
      } else if (c == '\'') {
        get_quoted_string();
      } else {
        assert(c != '"');
        get();
      }
    }
  }
  // Example: 'It\'s a string\\'
  string get_quoted_string() {
    assert(peek() == '\'');
    get();
    string out;
    char c;
    while ((c=get()) != '\'') {
      if (c == '\\')
        out += get();
      else
        out += c;
    }
    return out;
  }
  // Number example: -34.254e-2
  string get_number() {
    string out;
    if (peek() == '-')
      out += get();
    while (isdigit(peek())) {
      out += get();
    }
    if (peek() == '.') {
      out += get();
      while (isdigit(peek())) {
        out += get();
      }
    }
    if (peek() == 'e') {
      out += get();
      if (peek() == '-')
        out += get();
      while (isdigit(peek()))
        out += get();
    }
    return out;
  }
  void skip_after(char c1) {
    while (get() != c1 && !eof());
  }
  void skip_after(char c1, char c2) {
    char prev = get(), current = get();
    while ((prev != c1 || current != c2) && !eof()) {
      prev = current;
      current = get();
    }
  }
 private:
  FileReader<char> *f_;
  DataHandler *handler_;
 private:
  DISALLOW_COPY_AND_ASSIGN(SqlParser);
};

}  // namespace wikigraph

#endif  // SRC_SQL_PARSER_H_

