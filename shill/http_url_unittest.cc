//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
                  HttpUrl::Protocol in_protocol,
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
  HttpUrl::Protocol protocol;
  string host;
  int port;
  string path;
};

class HttpUrlParseTest : public testing::TestWithParam<StringAndResult> {
 protected:
  HttpUrl url_;
};

TEST_P(HttpUrlParseTest, ParseURL) {
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
    HttpUrlParseStringsTest,
    HttpUrlParseTest,
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
                        HttpUrl::kProtocolHTTP,
                        "www.foo.com",
                        HttpUrl::kDefaultHTTPPort,
                        "/"),
        StringAndResult("https://www.foo.com", true,
                        HttpUrl::kProtocolHTTPS,
                        "www.foo.com",
                        HttpUrl::kDefaultHTTPSPort,
                        "/"),
        StringAndResult("https://www.foo.com:4443", true,
                        HttpUrl::kProtocolHTTPS,
                        "www.foo.com",
                        4443,
                        "/"),
        StringAndResult("http://www.foo.com/bar", true,
                        HttpUrl::kProtocolHTTP,
                        "www.foo.com",
                        HttpUrl::kDefaultHTTPPort,
                        "/bar"),
        StringAndResult("http://www.foo.com?bar", true,
                        HttpUrl::kProtocolHTTP,
                        "www.foo.com",
                        HttpUrl::kDefaultHTTPPort,
                        "/?bar")));

}  // namespace shill
