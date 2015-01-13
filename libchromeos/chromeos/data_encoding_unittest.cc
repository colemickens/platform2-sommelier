// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/data_encoding.h>

#include <gtest/gtest.h>

namespace chromeos {
namespace data_encoding {

TEST(data_encoding, UrlEncoding) {
  std::string test = "\"http://sample/path/0014.html \"";
  std::string encoded = UrlEncode(test.c_str());
  EXPECT_EQ("%22http%3A%2F%2Fsample%2Fpath%2F0014.html+%22", encoded);
  EXPECT_EQ(test, UrlDecode(encoded.c_str()));

  test = "\"http://sample/path/0014.html \"";
  encoded = UrlEncode(test.c_str(), false);
  EXPECT_EQ("%22http%3A%2F%2Fsample%2Fpath%2F0014.html%20%22", encoded);
  EXPECT_EQ(test, UrlDecode(encoded.c_str()));
}

TEST(data_encoding, WebParamsEncoding) {
  std::string encoded =
      WebParamsEncode({{"q", "test"}, {"path", "/usr/bin"}, {"#", "%"}});
  EXPECT_EQ("q=test&path=%2Fusr%2Fbin&%23=%25", encoded);

  auto params = WebParamsDecode(encoded);
  EXPECT_EQ(3, params.size());
  EXPECT_EQ("q", params[0].first);
  EXPECT_EQ("test", params[0].second);
  EXPECT_EQ("path", params[1].first);
  EXPECT_EQ("/usr/bin", params[1].second);
  EXPECT_EQ("#", params[2].first);
  EXPECT_EQ("%", params[2].second);
}

}  // namespace data_encoding
}  // namespace chromeos
