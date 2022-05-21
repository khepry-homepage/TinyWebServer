#include <gtest/gtest.h>
#include "./test_http_parser.cpp"


int main(int argc, char** argv) {
  testing::InitGoogleTest();
  
  return RUN_ALL_TESTS();
}