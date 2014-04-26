// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_HTTP_CONNECTION_FAKE_H_
#define BUFFET_HTTP_CONNECTION_FAKE_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>

#include "buffet/http_connection.h"
#include "buffet/http_transport_fake.h"

namespace chromeos {
namespace http {
namespace fake {

// This is a fake implementation of http::Connection for unit testing.
class Connection : public chromeos::http::Connection {
 public:
  Connection(const std::string& url, const std::string& method,
             std::shared_ptr<http::Transport> transport);
  virtual ~Connection();

  // Overrides from http::Connection.
  // See http_connection.h for description of these methods.
  virtual bool SendHeaders(const HeaderList& headers) override;
  virtual bool WriteRequestData(const void* data, size_t size) override;
  virtual bool FinishRequest() override;

  virtual int GetResponseStatusCode() const override;
  virtual std::string GetResponseStatusText() const override;
  virtual std::string GetProtocolVersion() const override;
  virtual std::string GetResponseHeader(
     const std::string& header_name) const override;
  virtual uint64_t GetResponseDataSize() const override;
  virtual bool ReadResponseData(void* data, size_t buffer_size,
                                size_t* size_read) override;
  virtual std::string GetErrorMessage() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Connection);

  // Request and response objects passed to the user-provided request handler
  // callback. The request object contains all the request information.
  // The response object is the server response that is created by
  // the handler in response to the request.
  ServerRequest request_;
  ServerResponse response_;

  // Internal read data pointer needed for ReadResponseData() implementation.
  size_t response_data_ptr_ = 0;
};

}  // namespace fake
}  // namespace http
}  // namespace chromeos

#endif // BUFFET_HTTP_CONNECTION_FAKE_H_
