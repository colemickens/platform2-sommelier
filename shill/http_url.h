// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_HTTP_URL_H_
#define SHILL_HTTP_URL_H_

#include <base/macros.h>

#include <string>

namespace shill {

// Simple URL parsing class.
class HTTPURL {
 public:
  enum Protocol {
    kProtocolUnknown,
    kProtocolHTTP,
    kProtocolHTTPS
  };

  static const int kDefaultHTTPPort;
  static const int kDefaultHTTPSPort;

  HTTPURL();
  virtual ~HTTPURL();

  // Parse a URL from |url_string|.
  bool ParseFromString(const std::string& url_string);

  const std::string& host() const { return host_; }
  const std::string& path() const { return path_; }
  int port() const { return port_; }
  Protocol protocol() const { return protocol_; }

 private:
  static const char kDelimiters[];
  static const char kPortSeparator;
  static const char kPrefixHTTP[];
  static const char kPrefixHTTPS[];

  std::string host_;
  std::string path_;
  int port_;
  Protocol protocol_;

  DISALLOW_COPY_AND_ASSIGN(HTTPURL);
};

}  // namespace shill

#endif  // SHILL_HTTP_URL_H_
