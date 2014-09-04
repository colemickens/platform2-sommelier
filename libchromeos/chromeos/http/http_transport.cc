// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/http_transport.h>

#include <chromeos/http/http_transport_curl.h>

namespace chromeos {
namespace http {

const char kErrorDomain[] = "http_transport";

std::shared_ptr<Transport> Transport::CreateDefault() {
  return std::make_shared<http::curl::Transport>();
}

}  // namespace http
}  // namespace chromeos
