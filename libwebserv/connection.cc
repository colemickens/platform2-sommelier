// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libwebserv/connection.h"

#include <algorithm>
#include <vector>

#include <base/bind.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/task_runner.h>
#include <chromeos/http/http_request.h>
#include <microhttpd.h>

#include "libwebserv/request.h"
#include "libwebserv/request_handler_interface.h"
#include "libwebserv/response.h"
#include "libwebserv/server.h"

namespace libwebserv {

// Helper class to provide static callback methods to microhttpd library,
// with the ability to access private methods of Connection class.
class ConnectionHelper {
 public:
  static int PostDataIterator(void* cls,
                              MHD_ValueKind kind,
                              const char* key,
                              const char* filename,
                              const char* content_type,
                              const char* transfer_encoding,
                              const char* data,
                              uint64_t off,
                              size_t size) {
    Connection* server_connection = reinterpret_cast<Connection*>(cls);
    if (!server_connection->ProcessPostData(
        key, filename, content_type, transfer_encoding, data, off, size)) {
      return MHD_NO;
    }
    return MHD_YES;
  }
};

Connection::Connection(const scoped_refptr<base::TaskRunner>& task_runner,
                       MHD_Connection* connection,
                       RequestHandlerInterface* handler)
    : task_runner_(task_runner),
      raw_connection_(connection),
      handler_(handler) {
}

Connection::~Connection() {
  if (post_processor_)
    MHD_destroy_post_processor(post_processor_);
}

std::shared_ptr<Connection> Connection::Create(
      Server* server,
      const std::string& url,
      const std::string& method,
      MHD_Connection* connection,
      RequestHandlerInterface* handler) {
  std::shared_ptr<Connection> result(
      new Connection(server->task_runner_, connection, handler));
  VLOG(1) << "Incoming HTTP connection (" << result.get() << ")."
          << " Method='" << method << "', URL='" << url << "'";
  result->post_processor_ = MHD_create_post_processor(
      connection, 1024, &ConnectionHelper::PostDataIterator, result.get());
  result->request_ = Request::Create(result, url, method);
  result->response_ = Response::Create(result);
  return result;
}

bool Connection::BeginRequestData() {
  return request_->BeginRequestData();
}

bool Connection::AddRequestData(const void* data, size_t size) {
  if (!post_processor_)
    return request_->AddRawRequestData(data, size);
  return MHD_post_process(post_processor_,
                          static_cast<const char*>(data), size) == MHD_YES;
}

void Connection::EndRequestData() {
  if (request_processed_)
    return;

  request_->EndRequestData();
  // libmicrohttpd calls handlers on its own thread.
  // Redirect this to the main IO thread of the server.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RequestHandlerInterface::HandleRequest,
                  base::Unretained(handler_),
                  RequestPtr{std::move(request_)},
                  ResponsePtr{std::move(response_)}));
  request_processed_ = true;
}

bool Connection::ProcessPostData(const char* key,
                                 const char* filename,
                                 const char* content_type,
                                 const char* transfer_encoding,
                                 const char* data,
                                 uint64_t off,
                                 size_t size) {
  if (off == 0)
    return  request_->AddPostFieldData(key, filename, content_type,
                                       transfer_encoding, data, size);
  return request_->AppendPostFieldData(key, data, size);
}

}  // namespace libwebserv
