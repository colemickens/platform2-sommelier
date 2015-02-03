// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libwebserv/connection.h>

#include <algorithm>
#include <vector>

#include <base/bind.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/task_runner.h>
#include <chromeos/http/http_request.h>
#include <libwebserv/request.h>
#include <libwebserv/request_handler_interface.h>
#include <libwebserv/response.h>
#include <libwebserv/server.h>
#include <microhttpd.h>

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

// Helper class to provide static callback methods to microhttpd library,
// with the ability to access private methods of Request class.
class RequestHelper {
 public:
  static int ValueCallback(void* cls,
                           MHD_ValueKind kind,
                           const char* key,
                           const char* value) {
    auto self = reinterpret_cast<Request*>(cls);
    std::string data;
    if (value)
      data = value;
    if (kind == MHD_HEADER_KIND) {
      self->headers_.emplace(Request::GetCanonicalHeaderName(key), data);
    } else if (kind == MHD_COOKIE_KIND) {
      // TODO(avakulenko): add support for cookies...
    } else if (kind == MHD_POSTDATA_KIND) {
      self->post_data_.emplace(key, data);
    } else if (kind == MHD_GET_ARGUMENT_KIND) {
      self->get_data_.emplace(key, data);
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

scoped_refptr<Connection> Connection::Create(Server* server,
                                             const std::string& url,
                                             const std::string& method,
                                             MHD_Connection* connection,
                                             RequestHandlerInterface* handler) {
  scoped_refptr<Connection> result(
      new Connection(server->task_runner_, connection, handler));
  VLOG(1) << "Incoming HTTP connection (" << result.get() << ")."
          << " Method='" << method << "', URL='" << url << "'";
  result->post_processor_ = MHD_create_post_processor(
      connection, 1024, &ConnectionHelper::PostDataIterator, result.get());
  result->request_ = Request::Create(url, method);
  result->response_ = Response::Create(result);
  return result;
}

bool Connection::BeginRequestData() {
  MHD_get_connection_values(raw_connection_, MHD_HEADER_KIND,
                            &RequestHelper::ValueCallback, request_.get());
  MHD_get_connection_values(raw_connection_, MHD_COOKIE_KIND,
                            &RequestHelper::ValueCallback, request_.get());
  MHD_get_connection_values(raw_connection_, MHD_POSTDATA_KIND,
                            &RequestHelper::ValueCallback, request_.get());
  MHD_get_connection_values(raw_connection_, MHD_GET_ARGUMENT_KIND,
                            &RequestHelper::ValueCallback, request_.get());
  return true;
}

bool Connection::AddRequestData(const void* data, size_t size) {
  if (!post_processor_)
    return request_->AddRawRequestData(data, size);
  return MHD_post_process(post_processor_,
                          static_cast<const char*>(data), size) == MHD_YES;
}

void Connection::EndRequestData() {
  if (state_ == State::kIdle) {
    state_ = State::kRequestSent;
    // libmicrohttpd calls handlers on its own thread.
    // Redirect this to the main IO thread of the server.
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RequestHandlerInterface::HandleRequest,
                   base::Unretained(handler_), base::Passed(&request_),
                   base::Passed(&response_)));
  } else if (state_ == State::kResponseReceived) {
    VLOG(1) << "Sending HTTP response for connection (" << this
            << "): " << response_status_code_
            << ", data size = " << response_data_.size();
    MHD_Response* resp = MHD_create_response_from_buffer(
        response_data_.size(), response_data_.data(), MHD_RESPMEM_PERSISTENT);
    for (const auto& pair : response_headers_) {
      MHD_add_response_header(resp, pair.first.c_str(), pair.second.c_str());
    }
    CHECK_EQ(MHD_YES,
             MHD_queue_response(raw_connection_, response_status_code_, resp))
        << "Failed to queue response";
    MHD_destroy_response(resp);  // |resp| is ref-counted.
    state_ = State::kDone;
  }
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
