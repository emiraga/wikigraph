// Copyright 2011 Emir Habul, see file COPYING

#include <cstdio>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "sql_parser.h"
#include "tests/mock_file_io.h"

using ::testing::_;
using ::testing::Truly;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::ElementsAreArray;

namespace wikigraph {

class MockHandler : public DataHandler {
 public:
  MockHandler() { }
  MOCK_METHOD1(data, void(const vector<string>&));
};

TEST(SqlParser, simple) {
  char data[] = "INSERT INTO `page` VALUES (1,0,'Main_Page','',3),"
    "(2,0,'Main_\\'Page\\\\','',1,0,1);"
    "/* comment */\n -- comment\n"
    "INSERT INTO `page` VALUES ('3');\n\n";
  StubFile fs(data, sizeof(data));
  BufferedReader<char> b(&fs);

  string data1[] = {"1", "0", "Main_Page", "", "3"};
  string data2[] = {"2", "0", "Main_\'Page\\", "", "1", "0", "1"};
  string data3[] = {"3"};

  InSequence seq;
  MockHandler h;
  EXPECT_CALL(h, data(ElementsAreArray(data1))).Times(1);
  EXPECT_CALL(h, data(ElementsAreArray(data2))).Times(1);
  EXPECT_CALL(h, data(ElementsAreArray(data3))).Times(1);

  SqlParser s(&b, &h);
  s.run();
}

}  // namespace wikigraph

