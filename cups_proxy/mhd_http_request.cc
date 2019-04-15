// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cups_proxy/mhd_http_request.h"

#include <algorithm>
#include <utility>

namespace printing {

MHDHttpRequest::MHDHttpRequest() : chunked_(false) {}

void MHDHttpRequest::SetStatusLine(base::StringPiece method,
                                   base::StringPiece url,
                                   base::StringPiece version) {
  method_ = method.as_string();
  url_ = url.as_string();
  version_ = version.as_string();
}

void MHDHttpRequest::AddHeader(base::StringPiece key, base::StringPiece value) {
  // strip 100-continue message from ipp request
  if (key == "Expect" && value == "100-continue") {
    return;
  }

  // strip chunked header from ipp request
  if (key == "Transfer-Encoding" && value == "chunked") {
    chunked_ = true;
    return;
  }

  headers_[key.as_string()] = value.as_string();
}

void MHDHttpRequest::Finalize() {
  if (chunked_) {
    AddHeader("Content-Length", std::to_string(body_.size()));
  }
}

void MHDHttpRequest::PushToBody(base::StringPiece data) {
  body_.insert(body_.end(), data.begin(), data.end());
}

}  // namespace printing
