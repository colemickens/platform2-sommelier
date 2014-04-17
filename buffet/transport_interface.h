// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_TRANSPORT_INTERFACE_H_
#define BUFFET_TRANSPORT_INTERFACE_H_

#include <vector>
#include <string>
#include <base/basictypes.h>

namespace chromeos {
namespace http {

typedef std::vector<std::pair<std::string, std::string>> HeaderList;

///////////////////////////////////////////////////////////////////////////////
// TransportInterface is an interface to abstract specific implementation
// of HTTP communication. This interface (and its underlying implementation)
// is used by http::Request and http::Response classes to provide HTTP
// functionality to the clients. This interface should be of no interest
// to the clients unless they want to implement/use their own network library.
///////////////////////////////////////////////////////////////////////////////
class TransportInterface {
 public:
  enum class Stage {
    initialized,
    response_received,
    failed,
    closed
  };

  virtual ~TransportInterface() {}

  virtual Stage GetStage() const = 0;

  virtual void AddRange(int64_t bytes) = 0;
  virtual void AddRange(uint64_t from_byte, uint64_t to_byte) = 0;

  virtual void SetAccept(const char* accept_mime_types) = 0;
  virtual std::string GetAccept() const = 0;

  virtual std::string GetRequestURL() const = 0;

  virtual void SetContentType(const char* content_type) = 0;
  virtual std::string GetContentType() const = 0;

  virtual void AddHeader(const char* header, const char* value) = 0;
  virtual void RemoveHeader(const char* header) = 0;

  virtual bool AddRequestBody(const void* data, size_t size) = 0;

  virtual void SetMethod(const char* method) = 0;
  virtual std::string GetMethod() const = 0;

  virtual void SetReferer(const char* referer) = 0;
  virtual std::string GetReferer() const = 0;

  virtual void SetUserAgent(const char* user_agent) = 0;
  virtual std::string GetUserAgent() const = 0;

  virtual bool Perform() = 0;

  virtual int GetResponseStatusCode() const = 0;
  virtual std::string GetResponseStatusText() const = 0;

  virtual std::string GetResponseHeader(const char* header_name) const = 0;
  virtual const std::vector<unsigned char>& GetResponseData() const = 0;
  virtual std::string GetErrorMessage() const = 0;

  virtual void Close() = 0;

 protected:
  TransportInterface() {}
  DISALLOW_COPY_AND_ASSIGN(TransportInterface);
};

} // namespace http
} // namespace chromeos

#endif // BUFFET_TRANSPORT_INTERFACE_H_
