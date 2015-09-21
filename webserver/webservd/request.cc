// Copyright 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "webservd/request.h"

#include <microhttpd.h>

#include <base/bind.h>
#include <base/files/file.h>
#include <base/guid.h>
#include <chromeos/http/http_request.h>
#include <chromeos/http/http_utils.h>
#include <chromeos/mime_utils.h>
#include <chromeos/streams/file_stream.h>
#include <chromeos/strings/string_utils.h>
#include "webservd/log_manager.h"
#include "webservd/protocol_handler.h"
#include "webservd/request_handler_interface.h"
#include "webservd/server_interface.h"
#include "webservd/temp_file_manager.h"

namespace webservd {

// Helper class to provide static callback methods to microhttpd library,
// with the ability to access private methods of Request class.
class RequestHelper {
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
    auto self = reinterpret_cast<Request*>(cls);
    return self->ProcessPostData(key, filename, content_type, transfer_encoding,
                                 data, off, size) ? MHD_YES : MHD_NO;
  }

  static int ValueCallback(void* cls,
                           MHD_ValueKind kind,
                           const char* key,
                           const char* value) {
    auto self = reinterpret_cast<Request*>(cls);
    std::string data;
    if (value)
      data = value;
    if (kind == MHD_HEADER_KIND) {
      self->headers_.emplace_back(chromeos::http::GetCanonicalHeaderName(key),
                                  data);
    } else if (kind == MHD_COOKIE_KIND) {
      // TODO(avakulenko): add support for cookies...
    } else if (kind == MHD_POSTDATA_KIND) {
      self->post_data_.emplace_back(key, data);
    } else if (kind == MHD_GET_ARGUMENT_KIND) {
      self->get_data_.emplace_back(key, data);
    }
    return MHD_YES;
  }
};

FileInfo::FileInfo(const std::string& in_field_name,
                   const std::string& in_file_name,
                   const std::string& in_content_type,
                   const std::string& in_transfer_encoding)
    : field_name(in_field_name),
      file_name(in_file_name),
      content_type(in_content_type),
      transfer_encoding(in_transfer_encoding) {
}

Request::Request(
    const std::string& request_handler_id,
    const std::string& url,
    const std::string& method,
    const std::string& version,
    MHD_Connection* connection,
    ProtocolHandler* protocol_handler)
    : id_{base::GenerateGUID()},
      request_handler_id_{request_handler_id},
      url_{url},
      method_{method},
      version_{version},
      connection_{connection},
      protocol_handler_{protocol_handler} {
  // Here we create the data pipe used to transfer the request body from the
  // web server to the remote request handler.
  int pipe_fds[2] = {-1, -1};
  CHECK_EQ(0, pipe(pipe_fds));
  raw_data_pipe_in_ = base::File{pipe_fds[1]};
  CHECK(raw_data_pipe_in_.IsValid());
  raw_data_pipe_out_ = base::File{pipe_fds[0]};
  CHECK(raw_data_pipe_out_.IsValid());
  raw_data_stream_in_ = chromeos::FileStream::FromFileDescriptor(
      raw_data_pipe_in_.GetPlatformFile(), false, nullptr);
  CHECK(raw_data_stream_in_);

  // POST request processor.
  post_processor_ = MHD_create_post_processor(
      connection, 1024, &RequestHelper::PostDataIterator, this);
}

Request::~Request() {
  if (post_processor_)
    MHD_destroy_post_processor(post_processor_);
  GetTempFileManager()->DeleteRequestTempFiles(id_);
  protocol_handler_->RemoveRequest(this);
}

bool Request::GetFileData(int file_id, int* contents_fd) {
  if (file_id < 0 || static_cast<size_t>(file_id) >= file_info_.size())
    return false;
  base::File file(file_info_[file_id]->temp_file_name,
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return false;
  *contents_fd = file.TakePlatformFile();
  return true;
}

bool Request::Complete(
    int32_t status_code,
    const std::vector<std::tuple<std::string, std::string>>& headers,
    const std::vector<uint8_t>& data) {
  if (response_data_started_)
    return false;

  response_status_code_ = status_code;
  response_headers_.reserve(headers.size());
  for (const auto& tuple : headers) {
    response_headers_.emplace_back(std::get<0>(tuple), std::get<1>(tuple));
  }
  response_data_ = data;
  response_data_started_ = true;
  const MHD_ConnectionInfo* info =
      MHD_get_connection_info(connection_, MHD_CONNECTION_INFO_CLIENT_ADDRESS);

  const sockaddr* client_addr = (info ? info->client_addr : nullptr);
  LogManager::OnRequestCompleted(base::Time::Now(), client_addr, method_, url_,
                                 version_, status_code, data.size());
  protocol_handler_->ScheduleWork();
  return true;
}

bool Request::Complete(
    int32_t status_code,
    const std::vector<std::tuple<std::string, std::string>>& headers,
    const std::string& mime_type,
    const std::string& data) {
  std::vector<std::tuple<std::string, std::string>> headers_copy;
  headers_copy.emplace_back(chromeos::http::response_header::kContentType,
                            mime_type);
  return Complete(status_code, headers_copy,
                  chromeos::string_utils::GetStringAsBytes(data));
}

const std::string& Request::GetProtocolHandlerID() const {
  return protocol_handler_->GetID();
}

int Request::GetBodyDataFileDescriptor() const {
  int fd = dup(raw_data_pipe_out_.GetPlatformFile());
  CHECK_GE(fd, 0);
  return fd;
}

bool Request::BeginRequestData() {
  MHD_get_connection_values(connection_, MHD_HEADER_KIND,
                            &RequestHelper::ValueCallback, this);
  MHD_get_connection_values(connection_, MHD_COOKIE_KIND,
                            &RequestHelper::ValueCallback, this);
  MHD_get_connection_values(connection_, MHD_POSTDATA_KIND,
                            &RequestHelper::ValueCallback, this);
  MHD_get_connection_values(connection_, MHD_GET_ARGUMENT_KIND,
                            &RequestHelper::ValueCallback, this);
  // If we have POST processor, then we are parsing the request ourselves and
  // we need to dispatch it to the handler only after all the data is parsed.
  // Otherwise forward the request immediately and let the handler read the
  // request data as needed.
  if (!post_processor_)
    ForwardRequestToHandler();
  return true;
}

bool Request::AddRequestData(const void* data, size_t* size) {
  if (!post_processor_)
    return AddRawRequestData(data, size);
  int result =
      MHD_post_process(post_processor_, static_cast<const char*>(data), *size);
  *size = 0;
  return result == MHD_YES;
}

void Request::EndRequestData() {
  if (!request_data_finished_) {
    if (raw_data_stream_in_) {
      raw_data_stream_in_->CloseBlocking(nullptr);
      raw_data_pipe_in_.Close();
    }
    if (!request_forwarded_)
      ForwardRequestToHandler();
    request_data_finished_ = true;
  }

  if (response_data_started_ && !response_data_finished_) {
    MHD_Response* resp = MHD_create_response_from_buffer(
        response_data_.size(), response_data_.data(), MHD_RESPMEM_PERSISTENT);
    for (const auto& pair : response_headers_) {
      MHD_add_response_header(resp, pair.first.c_str(), pair.second.c_str());
    }
    CHECK_EQ(MHD_YES,
             MHD_queue_response(connection_, response_status_code_, resp))
        << "Failed to queue response";
    MHD_destroy_response(resp);  // |resp| is ref-counted.
    response_data_finished_ = true;
  }
}

void Request::ForwardRequestToHandler() {
  request_forwarded_ = true;
  if (!request_handler_id_.empty()) {
    // Close all temporary file streams, if any.
    for (auto& file : file_info_)
      file->data_stream->CloseBlocking(nullptr);

    protocol_handler_->AddRequest(this);
    auto p = protocol_handler_->request_handlers_.find(request_handler_id_);
    CHECK(p != protocol_handler_->request_handlers_.end());
    // Send the request over D-Bus and await the response.
    p->second.handler->HandleRequest(this);
  } else {
    // There was no handler found when request was made, respond with
    // 404 Page Not Found.
    Complete(chromeos::http::status_code::NotFound, {},
             chromeos::mime::text::kPlain, "Not Found");
  }
}

bool Request::ProcessPostData(const char* key,
                              const char* filename,
                              const char* content_type,
                              const char* transfer_encoding,
                              const char* data,
                              uint64_t off,
                              size_t size) {
  if (off > 0)
    return AppendPostFieldData(key, data, size);

  return AddPostFieldData(key, filename, content_type, transfer_encoding, data,
                          size);
}

bool Request::AddRawRequestData(const void* data, size_t* size) {
  CHECK(*size);
  CHECK(raw_data_stream_in_) << "Data pipe hasn't been created.";

  size_t written = 0;
  if (!raw_data_stream_in_->WriteNonBlocking(data, *size, &written, nullptr))
    return false;

  CHECK_LE(written, *size);

  // If we didn't write all the data requested, we need to let libmicrohttpd do
  // another write cycle. Schedule a DoWork() action here.
  if (written != *size)
    protocol_handler_->ScheduleWork();

  *size -= written;

  // If written at least some data, we are good. We will be called again if more
  // data is available.
  if (written > 0)
    return true;

  // Nothing has been written. The output pipe is full. Need to stop the data
  // transfer on the connection and wait till some data is being read from the
  // pipe by the request handler.
  MHD_suspend_connection(connection_);

  // Now, just monitor the pipe and figure out when we can resume sending data
  // over it.
  bool success = raw_data_stream_in_->WaitForData(
      chromeos::Stream::AccessMode::WRITE,
      base::Bind(&Request::OnPipeAvailable, weak_ptr_factory_.GetWeakPtr()),
      nullptr);

  if (!success)
    MHD_resume_connection(connection_);

  return success;
}

void Request::OnPipeAvailable(chromeos::Stream::AccessMode mode) {
  CHECK(mode == chromeos::Stream::AccessMode::WRITE);
  MHD_resume_connection(connection_);
  protocol_handler_->ScheduleWork();
}

bool Request::AddPostFieldData(const char* key,
                               const char* filename,
                               const char* content_type,
                               const char* transfer_encoding,
                               const char* data,
                               size_t size) {
  if (filename) {
    std::unique_ptr<FileInfo> file_info{
        new FileInfo{key, filename, content_type ? content_type : "",
                     transfer_encoding ? transfer_encoding : ""}};
    file_info->temp_file_name = GetTempFileManager()->CreateTempFileName(id_);
    file_info->data_stream = chromeos::FileStream::Open(
        file_info->temp_file_name, chromeos::Stream::AccessMode::READ_WRITE,
        chromeos::FileStream::Disposition::CREATE_ALWAYS, nullptr);
    if (!file_info->data_stream ||
        !file_info->data_stream->WriteAllBlocking(data, size, nullptr)) {
      return false;
    }
    file_info_.push_back(std::move(file_info));
    last_posted_data_was_file_ = true;
    return true;
  }
  std::string value{data, size};
  post_data_.emplace_back(key, value);
  last_posted_data_was_file_ = false;
  return true;
}

bool Request::AppendPostFieldData(const char* key,
                                  const char* data,
                                  size_t size) {
  if (last_posted_data_was_file_) {
    CHECK(!file_info_.empty());
    CHECK(file_info_.back()->field_name == key);
    FileInfo* file_info = file_info_.back().get();
    return file_info->data_stream->WriteAllBlocking(data, size, nullptr);
  }

  CHECK(!post_data_.empty());
  CHECK(post_data_.back().first == key);
  post_data_.back().second.append(data, size);
  return true;
}

TempFileManager* Request::GetTempFileManager() {
  return protocol_handler_->GetServer()->GetTempFileManager();
}

}  // namespace webservd
