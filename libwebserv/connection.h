// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEBSERV_CONNECTION_H_
#define LIBWEBSERV_CONNECTION_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <chromeos/errors/error.h>

#include "libwebserv/export.h"

struct MHD_Connection;
struct MHD_PostProcessor;

namespace base {
class TaskRunner;
}  // namespace base

namespace libwebserv {

class Request;
class RequestHandlerInterface;
class Response;
class Server;

// A wrapper class around low-level HTTP connection.
class LIBWEBSERV_EXPORT Connection final {
 public:
  ~Connection();

  // Factory creator method. Creates an instance of the connection and
  // initializes some complex data members. This is safer and easier to
  // report possible failures than reply on just the constructor.
  static std::shared_ptr<Connection> Create(Server* server,
                                            const std::string& url,
                                            const std::string& method,
                                            MHD_Connection* connection,
                                            RequestHandlerInterface* handler);

 private:
  LIBWEBSERV_PRIVATE Connection(
      const scoped_refptr<base::TaskRunner>& task_runner,
      MHD_Connection* connection,
      RequestHandlerInterface* handler);

  // Helper callback methods used by Server's ConnectionHandler to transfer
  // request headers and data to the Connection's Request object.
  LIBWEBSERV_PRIVATE bool BeginRequestData();
  LIBWEBSERV_PRIVATE bool AddRequestData(const void* data, size_t size);
  LIBWEBSERV_PRIVATE void EndRequestData();

  // Callback for libmicrohttpd's PostProcessor.
  LIBWEBSERV_PRIVATE bool ProcessPostData(const char* key,
                                          const char* filename,
                                          const char* content_type,
                                          const char* transfer_encoding,
                                          const char* data,
                                          uint64_t off,
                                          size_t size);

  scoped_refptr<base::TaskRunner> task_runner_;
  MHD_Connection* raw_connection_{nullptr};
  RequestHandlerInterface* handler_{nullptr};
  MHD_PostProcessor* post_processor_{nullptr};
  std::unique_ptr<Request> request_;
  std::unique_ptr<Response> response_;
  bool request_processed_{false};

  friend class ConnectionHelper;
  friend class Request;
  friend class Response;
  friend class ServerHelper;
  DISALLOW_COPY_AND_ASSIGN(Connection);
};

}  // namespace libwebserv

#endif  // LIBWEBSERV_CONNECTION_H_
