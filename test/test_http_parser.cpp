#include "../http_parser/http_parser.cpp"
#include <iostream>

using namespace http;

TEST(HTTP_PARSER_TEST, PARSE_LINE_1) {
  char* text = "GET /562f25980001b1b106000338.jpg HTTP/1.1\r\n"
                  "Host:img.mukewang.com\r\n"
                  "User-Agent:Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36\r\n" 
                  "Accept:image/webp,image/*,*/*;q=0.8\r\n" 
                  "Referer:http://www.imooc.com/\r\n" 
                  "Accept-Encoding:gzip, deflate, sdch\r\n"
                  "Accept-Language:zh-CN,zh;q=0.8\r\n"
                  "\r\n"
                  "数据";
  HttpParser hp(text);
  std::string str;
  if (hp.parseLine(str) == LINE_BAD)  return;
  EXPECT_EQ(str, "GET /562f25980001b1b106000338.jpg HTTP/1.1");
  if (hp.parseLine(str) == LINE_BAD)  return;
  EXPECT_EQ(str, "Host:img.mukewang.com");
  if (hp.parseLine(str) == LINE_BAD)  return;
  EXPECT_EQ(str, "User-Agent:Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36");

  if (hp.parseLine(str) == LINE_BAD)  return;
  EXPECT_EQ(str, "Accept:image/webp,image/*,*/*;q=0.8");
  if (hp.parseLine(str) == LINE_BAD)  return;
  EXPECT_EQ(str, "Referer:http://www.imooc.com/");
  if (hp.parseLine(str) == LINE_BAD)  return;
  EXPECT_EQ(str, "Accept-Encoding:gzip, deflate, sdch");
  if (hp.parseLine(str) == LINE_BAD)  return;
  EXPECT_EQ(str, "Accept-Language:zh-CN,zh;q=0.8");
  if (hp.parseLine(str) == LINE_BAD)  return;
  EXPECT_EQ(str, "");
  if (hp.parseLine(str) == LINE_BAD)  return;
  EXPECT_EQ(str, "数据");
}

