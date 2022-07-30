#include "../include/http_conn.h"
#include <string>
using namespace std;
#include <cstdio>


TEST(HTTP_PARSER_TEST, PROCESS_READ) {
  char text[] = "GET /562f25980001b1b106000338.jpg HTTP/1.1\r\n"
                "Host: img.mukewang.com\r\n"
                "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36\r\n" 
                "Accept: image/webp,image/*,*/*;q=0.8\r\n" 
                "Referer: http://www.imooc.com/\r\n" 
                "Accept-Encoding: gzip, deflate, sdch\r\n"
                "Accept-Language: zh-CN,zh;q=0.8\r\n"
                "\r\n";
  char err_text1[128] = "GET /562f25980001b1b106000338.jpg HTTP/1.1\r"
                        "\nHost: img.mukewang.com\r\n";
  char err_text2[128] = "GET /562f25980001b1b106000338.jpg HTTP/1.1\n";
  HttpConn hc;
  HttpRequest* hr = hc.GetHR();
  hc.CopyReadBuf(text, 44);
  ASSERT_TRUE(hc.ProcessRead() == HttpConn::NO_REQUEST);
  EXPECT_EQ(hr->method, HttpRequest::GET);
  EXPECT_EQ(string(hr->uri), "562f25980001b1b106000338.jpg");
  EXPECT_EQ(hr->version, HttpRequest::HTTP_11);
  hc.CopyReadBuf(text + 44, 24);
  ASSERT_TRUE(hc.ProcessRead() == HttpConn::NO_REQUEST);
  EXPECT_EQ(string(hr->header_option[HttpRequest::Host]), "img.mukewang.com");
  hc.CopyReadBuf(text + 68, 124);
  ASSERT_TRUE(hc.ProcessRead() == HttpConn::NO_REQUEST);
  EXPECT_EQ(string(hr->header_option[HttpRequest::User_Agent]), "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36");
  hc.CopyReadBuf(text + 192, 38);
  ASSERT_TRUE(hc.ProcessRead() == HttpConn::NO_REQUEST);
  EXPECT_EQ(string(hr->header_option[HttpRequest::Accept]), "image/webp,image/*,*/*;q=0.8");
  hc.CopyReadBuf(text + 230, 32);
  ASSERT_TRUE(hc.ProcessRead() == HttpConn::NO_REQUEST);
  EXPECT_EQ(string(hr->header_option[HttpRequest::Referer]), "http://www.imooc.com/");
  hc.CopyReadBuf(text + 262, 38);
  ASSERT_TRUE(hc.ProcessRead() == HttpConn::NO_REQUEST);
  EXPECT_EQ(string(hr->header_option[HttpRequest::Accept_Encoding]), "gzip, deflate, sdch");
  hc.CopyReadBuf(text + 300, 33);
  ASSERT_TRUE(hc.ProcessRead() == HttpConn::NO_REQUEST);
  EXPECT_EQ(string(hr->header_option[HttpRequest::Accept_Language]), "zh-CN,zh;q=0.8");
  hc.CopyReadBuf(text + 333, 2);
  ASSERT_TRUE(hc.ProcessRead() == HttpConn::GET_REQUEST);
  HttpConn::Init(hc.GetHR(), &hc);
  hc.CopyReadBuf(err_text1, 43);
  ASSERT_TRUE(hc.ProcessRead() == HttpConn::NO_REQUEST);
  hc.CopyReadBuf(err_text1 + 43, 25);
  ASSERT_TRUE(hc.ProcessRead() == HttpConn::NO_REQUEST);
  HttpConn::Init(hc.GetHR(), &hc);
  hc.CopyReadBuf(err_text2, strlen(err_text2) + 1);
  ASSERT_TRUE(hc.ProcessRead() == HttpConn::BAD_REQUEST);
}
