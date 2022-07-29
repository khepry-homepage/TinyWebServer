#include "../http/http_parser.cpp"
#include <string>
using namespace std;
#include <cstdio>

TEST(HTTP_PARSER_TEST, PARSE_LINE) {
  char text[] = "GET /562f25980001b1b106000338.jpg HTTP/1.1\r\n"
                "Host:img.mukewang.com\r\n"
                "User-Agent:Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36\r\n" 
                "Accept:image/webp,image/*,*/*;q=0.8\r\n" 
                "Referer:http://www.imooc.com/\r\n" 
                "Accept-Encoding:gzip, deflate, sdch\r\n"
                "Accept-Language:zh-CN,zh;q=0.8\r\n"
                "\r\n";
  char err_text1[128] = "GET /562f25980001b1b106000338.jpg HTTP/1.1\r"
                        "\nHost:img.mukewang.com\r\n";
  char err_text2[128] = "GET /562f25980001b1b106000338.jpg HTTP/1.1\n";
  int read_idx = 46;
  int check_idx = 0;
  char *line = text;
  ASSERT_FALSE(HttpParser::ParseLine(text, read_idx, check_idx) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "GET /562f25980001b1b106000338.jpg HTTP/1.1");
  line = text + check_idx;
  read_idx = 71;
  ASSERT_FALSE(HttpParser::ParseLine(text, read_idx, check_idx) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "Host:img.mukewang.com");
  line = text + check_idx;
  read_idx = 196;
  ASSERT_FALSE(HttpParser::ParseLine(text, read_idx, check_idx) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "User-Agent:Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36");
  line = text + check_idx;
  read_idx = 235;
  ASSERT_FALSE(HttpParser::ParseLine(text, read_idx, check_idx) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "Accept:image/webp,image/*,*/*;q=0.8");
  line = text + check_idx;
  read_idx = 268;
  ASSERT_FALSE(HttpParser::ParseLine(text, read_idx, check_idx) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "Referer:http://www.imooc.com/");
  line = text + check_idx;
  read_idx = 307;
  ASSERT_FALSE(HttpParser::ParseLine(text, read_idx, check_idx) == HttpParser::LINE_BAD);
  EXPECT_EQ(string(line), "Accept-Encoding:gzip, deflate, sdch");
  line = text + check_idx;
  read_idx = 341;
  ASSERT_TRUE(HttpParser::ParseLine(text, read_idx, check_idx) == HttpParser::LINE_OK);
  EXPECT_EQ(string(line), "Accept-Language:zh-CN,zh;q=0.8");
  line = text + check_idx;
  read_idx = 343;
  ASSERT_TRUE(HttpParser::ParseLine(text, read_idx, check_idx) == HttpParser::LINE_OK);
  EXPECT_EQ(string(line), "");
  line = text + check_idx;
  read_idx = 43;
  check_idx = 0;
  ASSERT_TRUE(HttpParser::ParseLine(err_text1, read_idx, check_idx) == HttpParser::LINE_MORE);
  ASSERT_TRUE(check_idx == 42);
  read_idx = 128;
  ASSERT_TRUE(HttpParser::ParseLine(err_text1, read_idx, check_idx) == HttpParser::LINE_OK);
  ASSERT_TRUE(check_idx == 44);
  check_idx = 0;
  ASSERT_TRUE(HttpParser::ParseLine(err_text2, read_idx, check_idx) == HttpParser::LINE_BAD);
  ASSERT_TRUE(check_idx == 42);
}

TEST(HTTP_PARSER_TEST, PROCESS_READ) {
  char text1[] = "GET /ind.html HTTP/1.1\r\n"
                "Host:img.mukewang.com\r\n"
                "User-Agent:Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36\r\n" 
                "Accept:image/webp,image/*,*/*;q=0.8\r\n" 
                "Referer:http://www.imooc.com/\r\n" 
                "Accept-Encoding:gzip, deflate, sdch\r\n"
                "Accept-Language:zh-CN,zh;q=0.8\r\n"
                "\r\n";
  int read_idx = 400;
  int check_idx = 0;
  int start_idx = 0;
  char *line = text1;
  HttpRequest hr1;
  HttpParser::HTTP_CODE code = HttpParser::ProcessRead(text1, read_idx, start_idx, check_idx, hr1);
  EXPECT_EQ(code, HttpParser::GET_REQUEST);
  EXPECT_EQ(hr1.method, HttpRequest::GET);
  EXPECT_EQ(string(hr1.uri), "ind.html");
  EXPECT_EQ(hr1.version, HttpRequest::HTTP_11);
  EXPECT_EQ(hr1.content, nullptr);

  char text2[] = "PUT /ind.html HTTP/1.1\r";
  read_idx = 23;
  start_idx = 0;
  check_idx = 0;
  HttpRequest hr2;
  code = HttpParser::ProcessRead(text2, read_idx, start_idx, check_idx, hr2);
  EXPECT_EQ(hr2.method, HttpRequest::METHOD_NOT_SUPPORT);
  EXPECT_EQ(code, HttpParser::NO_REQUEST);
  HttpRequest hr3;
  char text3[] = "POST /ind.html HTTP/1.1\r\n"
                 "Host: img.mukewang.com\r\n"
                 "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36\r\n";
  read_idx = 300;
  start_idx = 0;
  check_idx = 0;
  code = HttpParser::ProcessRead(text3, read_idx, start_idx, check_idx, hr3);
  EXPECT_EQ(code, HttpParser::NO_REQUEST);
  EXPECT_EQ(string(hr3.header_option[HttpRequest::Host]), "img.mukewang.com");
}
