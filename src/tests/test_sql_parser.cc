// Copyright 2011 Emir Habul, see file COPYING

#include <cstdio>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "sql_parser.h"
#include "tests/mock_file_io.h"
#include "tests/mock_graph.h"

using ::testing::_;
using ::testing::Truly;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::ElementsAreArray;

namespace wikigraph {

class MockHandler {
 public:
  MOCK_METHOD1(data, void(const vector<string>&));
};
typedef BufferedReader<StubFileSystem, char> StubCharReader;

TEST(SqlParser, simple) {
  char data[] = "INSERT INTO `page` VALUES (1,0,'Main_Page','',3),"
    "(2,0,'Main_\\'Page\\\\','',1,0,1);"
    "/* comment */\n -- comment\n"
    "INSERT INTO `page` VALUES ('3');\n\n";
  StubFileSystem fs(data, sizeof(data));
  StubCharReader b(&fs);

  string data1[] = {"1", "0", "Main_Page", "", "3"};
  string data2[] = {"2", "0", "Main_\'Page\\", "", "1", "0", "1"};
  string data3[] = {"3"};

  InSequence seq;
  MockHandler h;
  EXPECT_CALL(h, data(ElementsAreArray(data1))).Times(1);
  EXPECT_CALL(h, data(ElementsAreArray(data2))).Times(1);
  EXPECT_CALL(h, data(ElementsAreArray(data3))).Times(1);

  SqlParser<StubCharReader, MockHandler> s(&b, &h);
  s.run();
}

}  // namespace wikigraph

