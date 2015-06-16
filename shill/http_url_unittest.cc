// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/http_url.h"

#include <gtest/gtest.h>

using std::string;
using testing::Test;

namespace shill {

struct StringAndResult {
  StringAndResult(const string& in_url_string,
                  bool in_result)
      : url_string(in_url_string),
        result(in_result) {}
  StringAndResult(const string& in_url_string,
                  bool in_result,
                  HTTPURL::Protocol in_protocol,
                  const string& in_host,
                  int in_port,
                  const string& in_path)
      : url_string(in_url_string),
        result(in_result),
        protocol(in_protocol),
        host(in_host),
        port(in_port),
        path(in_path) {}
  string url_string;
  bool result;
  HTTPURL::Protocol protocol;
  string host;
  int port;
  string path;
};

class HTTPURLParseTest : public testing::TestWithParam<StringAndResult> {
 protected:
  HTTPURL url_;
};

TEST_P(HTTPURLParseTest, ParseURL) {
  bool result = url_.ParseFromString(GetParam().url_string);
  EXPECT_EQ(GetParam().result, result);
  if (GetParam().result && result) {
    EXPECT_EQ(GetParam().host, url_.host());
    EXPECT_EQ(GetParam().path, url_.path());
    EXPECT_EQ(GetParam().protocol, url_.protocol());
    EXPECT_EQ(GetParam().port, url_.port());
  }
}

INSTANTIATE_TEST_CASE_P(
    HTTPURLParseStringsTest,
    HTTPURLParseTest,
    ::testing::Values(
        StringAndResult("", false),                      // Empty string
        StringAndResult("xxx", false),                   // No known prefix
        StringAndResult(" http://www.foo.com", false),   // Leading garbage
        StringAndResult("http://", false),               // No hostname
        StringAndResult("http://:100", false),           // Port but no hostname
        StringAndResult("http://www.foo.com:", false),   // Colon but no port
        StringAndResult("http://www.foo.com:x", false),  // Non-numeric port
        StringAndResult("http://foo.com:10:20", false),  // Too many colons
        StringAndResult("http://www.foo.com", true,
                        HTTPURL::kProtocolHTTP,
                        "www.foo.com",
                        HTTPURL::kDefaultHTTPPort,
                        "/"),
        StringAndResult("https://www.foo.com", true,
                        HTTPURL::kProtocolHTTPS,
                        "www.foo.com",
                        HTTPURL::kDefaultHTTPSPort,
                        "/"),
        StringAndResult("https://www.foo.com:4443", true,
                        HTTPURL::kProtocolHTTPS,
                        "www.foo.com",
                        4443,
                        "/"),
        StringAndResult("http://www.foo.com/bar", true,
                        HTTPURL::kProtocolHTTP,
                        "www.foo.com",
                        HTTPURL::kDefaultHTTPPort,
                        "/bar"),
        StringAndResult("http://www.foo.com?bar", true,
                        HTTPURL::kProtocolHTTP,
                        "www.foo.com",
                        HTTPURL::kDefaultHTTPPort,
                        "/?bar")));

}  // namespace shill
