// Copyright 2011 Emir Habul, see file COPYING

#ifndef SRC_WRITERLIB_H_
#define SRC_WRITERLIB_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <numeric>

#include "wikigraph-stubs-internal.h"

namespace wikigraph {

const unsigned int kBufferSize = 1024*1024;  // for buffered reader and writer

class SystemFile {
 public:
  SystemFile() { }
  bool open(const char *path, const char *mode) {
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
 private:
  FILE *f_;
  DISALLOW_COPY_AND_ASSIGN(SystemFile);
};

// Write data to the file
template<class File>
class BufferedWriter {
 public:
  explicit BufferedWriter(File *f)
      :file_(f), pos_(0), bitpos_(0) { }

  ~BufferedWriter() {
    close();
  }

  void flush() {
    size_t res = file_->write(buffer_, 4, pos_);
    assert(res == pos_);
    pos_ = 0;
  }

  void close() {
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
    file_->close();
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
      :file_(f), read_size_(0), index_(0) {
  }

  ~BufferedReader() {
    close();
  }

  bool eof() const {
    return file_->eof();
  }

  void close() {
    if (file_ == NULL)
      return;

    file_->close();
    file_ = NULL;
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

template<class FileWriter>
class GraphWriter {
 public:
  GraphWriter(FileWriter *f, int num_nodes)
      : writer_(f), nodes_(num_nodes), cur_node_(0), file_pos_(0) {
    size_t list_len = 2 * (num_nodes + 1);
    list_ = new node_t[ list_len ];
    memset(list_, -1, sizeof(node_t) * list_len);
  }
  ~GraphWriter() {
    close();
  }
  // Tell the class that you are beginning to emit edges for a new node
  void start_node(node_t node) {
    assert(static_cast<int>(node) > 0);
    assert(static_cast<int>(node) <= nodes_);
    assert(node > cur_node_);  // Nodes must be given in increasing order
    if (PREDICT_FALSE(cur_node_ != 0)) {
      end(cur_node_) = file_pos_;
    }
    // each node should be written just once
    // node zero is always un-initialized
    assert(start(node) == start(0));
    start(node) = file_pos_;
    cur_node_ = node;
  }
  void add_edge(node_t edge) {
    assert(static_cast<int>(edge) > 0);
    assert(static_cast<int>(edge) <= nodes_);

    assert(cur_node_ != 0);
    writer_->write_uint(edge);
    file_pos_++;
  }
  void close() {
    if (writer_ == NULL)
      return;
    if (cur_node_ != 0) {
      end(cur_node_) = file_pos_;
    }
    cur_node_ = 0;

    int num_edges = file_pos_;
    size_t list_len = 2 * (nodes_ + 1);
    writer_->write(list_, 4, list_len);  // write list of nodes
    writer_->write_uint(num_edges);  // to the end of file, number of edges
    writer_->write_uint(nodes_);  // and number of nodes are added
    writer_->close();
    writer_ = NULL;
  }
 private:
  // Refers to beginning of edge list for each node
  node_t& start(node_t node) {
    return list_[2 * node];
  }
  // End of edge list
  node_t& end(node_t node) {
    return list_[2 * node + 1];
  }

  FileWriter *writer_;
  int nodes_;  // number of nodes
  node_t cur_node_;  // which node is currently active
  int file_pos_;  // nodes/edges not bytes
  node_t *list_;  // beginning and end of edge list for each node
 private:
  DISALLOW_COPY_AND_ASSIGN(GraphWriter);
};

}  // namespace wikigraph

#endif  // SRC_WRITERLIB_H_

