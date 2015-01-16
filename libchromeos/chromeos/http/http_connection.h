// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_HTTP_CONNECTION_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_HTTP_CONNECTION_H_

#include <memory>
#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/macros.h>
#include <chromeos/chromeos_export.h>
#include <chromeos/errors/error.h>
#include <chromeos/http/data_reader.h>
#include <chromeos/http/http_transport.h>

namespace chromeos {
namespace http {

class Response;

///////////////////////////////////////////////////////////////////////////////
// Connection class is the base class for HTTP communication session.
// It abstracts the implementation of underlying transport library (ex libcurl).
// When the Connection-derived class is constructed, it is pre-set up with
// basic initialization information necessary to initiate the server request
// connection (such as the URL, request method, etc - see
// Transport::CreateConnection() for more details). But most implementations
// would not probably initiate the physical connection until SendHeaders
// is called.
// You normally shouldn't worry about using this class directly.
// http::Request and http::Response classes use it for communication.
///////////////////////////////////////////////////////////////////////////////
class CHROMEOS_EXPORT Connection
    : public std::enable_shared_from_this<Connection> {
 public:
  explicit Connection(const std::shared_ptr<Transport>& transport)
      : transport_(transport) {}
  virtual ~Connection() = default;

  // Called by http::Request to initiate the connection with the server.
  // This normally opens the socket and sends the request headers.
  virtual bool SendHeaders(const HeaderList& headers,
                           chromeos::ErrorPtr* error) = 0;
  // If needed, this function can be called to send the request body data.
  virtual bool SetRequestData(std::unique_ptr<DataReaderInterface> data_reader,
                              chromeos::ErrorPtr* error) = 0;
  // This function is called when all the data is sent off and it's time
  // to receive the response data.
  virtual bool FinishRequest(chromeos::ErrorPtr* error) = 0;
  // Send the request asynchronously and invoke the callback with the response
  // received. Returns the ID of the pending async request.
  virtual int FinishRequestAsync(const SuccessCallback& success_callback,
                                 const ErrorCallback& error_callback) = 0;

  // Returns the HTTP status code (e.g. 200 for success).
  virtual int GetResponseStatusCode() const = 0;
  // Returns the status text (e.g. for error 403 it could be "NOT AUTHORIZED").
  virtual std::string GetResponseStatusText() const = 0;
  // Returns the HTTP protocol version (e.g. "HTTP/1.1").
  virtual std::string GetProtocolVersion() const = 0;
  // Returns the value of particular response header, or empty string if the
  // headers wasn't received.
  virtual std::string GetResponseHeader(
      const std::string& header_name) const = 0;
  // Returns the response data size, if known. For chunked (streaming)
  // transmission this might not be known until all the data is sent.
  // In this case GetResponseDataSize() will return 0.
  virtual uint64_t GetResponseDataSize() const = 0;
  // This function is called to read a block of response data.
  // It needs to be called repeatedly until it returns false or |size_read| is
  // set to 0. |data| is the destination buffer to read the data into.
  // |buffer_size| is the size of the buffer (amount of data to read).
  // |read_size| is the amount of data actually read, which could be less than
  // the size requested or 0 if there is no more data available.
  virtual bool ReadResponseData(void* data,
                                size_t buffer_size,
                                size_t* size_read,
                                chromeos::ErrorPtr* error) = 0;

 protected:
  // |transport_| is mainly used to keep the object alive as long as the
  // connection exists. But some implementations of Connection could use
  // the Transport-derived class for their own needs as well.
  std::shared_ptr<Transport> transport_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Connection);
};

}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_HTTP_CONNECTION_H_
