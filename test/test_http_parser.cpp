#include "../http_parser/http_parser.cpp"
#include <string>
using namespace http;
using namespace std;
TEST(HTTP_PARSER_TEST, PARSE_LINE_1) {
  char text[] = "GET /562f25980001b1b106000338.jpg HTTP/1.1\r\n"
                "Host:img.mukewang.com\r\n"
                "User-Agent:Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36\r\n" 
                "Accept:image/webp,image/*,*/*;q=0.8\r\n" 
                "Referer:http://www.imooc.com/\r\n" 
                "Accept-Encoding:gzip, deflate, sdch\r\n"
                "Accept-Language:zh-CN,zh;q=0.8\r\n"
                "\r\n";
  int index = 0;
  char* line = text;
  ASSERT_FALSE(HttpParser::ParseLine(text, index) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "GET /562f25980001b1b106000338.jpg HTTP/1.1");
  line = text + index;
  ASSERT_FALSE(HttpParser::ParseLine(text, index) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "Host:img.mukewang.com");
  line = text + index;
  ASSERT_FALSE(HttpParser::ParseLine(text, index) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "User-Agent:Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36");
  line = text + index;
  ASSERT_FALSE(HttpParser::ParseLine(text, index) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "Accept:image/webp,image/*,*/*;q=0.8");
  line = text + index;
  ASSERT_FALSE(HttpParser::ParseLine(text, index) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "Referer:http://www.imooc.com/");
  line = text + index;
  ASSERT_FALSE(HttpParser::ParseLine(text, index) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "Accept-Encoding:gzip, deflate, sdch");
  line = text + index;
  ASSERT_TRUE(HttpParser::ParseLine(text, index) == HttpParser::LINE_OK);
  EXPECT_EQ(string(line), "Accept-Language:zh-CN,zh;q=0.8");
  line = text + index;
  ASSERT_TRUE(HttpParser::ParseLine(text, index) == HttpParser::LINE_OK);
  EXPECT_EQ(string(line), "");
  line = text + index;
  ASSERT_TRUE(HttpParser::ParseLine(text, index) == HttpParser::LINE_MORE);
  EXPECT_NE(string(line), "数据");
}

