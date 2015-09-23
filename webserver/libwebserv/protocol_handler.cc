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

#include "libwebserv/protocol_handler.h"

#include <tuple>

#include <base/logging.h>
#include <chromeos/map_utils.h>
#include <chromeos/streams/file_stream.h>

#include "dbus_bindings/org.chromium.WebServer.RequestHandler.h"
#include "libwebserv/request.h"
#include "libwebserv/request_handler_callback.h"
#include "libwebserv/response.h"
#include "libwebserv/server.h"
#include "webservd/dbus-proxies.h"

namespace libwebserv {

namespace {

// Dummy callbacks for async D-Bus/Stream errors.
void IgnoreDBusError(chromeos::Error* error) {}
void IgnoreStreamError(const chromeos::Error* error) {}

// Structure to hold the data needed for asynchronous copying of two streams.
struct StreamCopyData {
  chromeos::StreamPtr src_stream;
  chromeos::StreamPtr dest_stream;
  std::vector<uint8_t> buffer;
};

// Forward-declaration.
void PerformRead(std::unique_ptr<StreamCopyData> data);

// Async callback which writes data to the destination stream after read.
void PerformWrite(std::unique_ptr<StreamCopyData> data, size_t size) {
  if (size == 0) // We are all done.
    return;
  data->dest_stream->WriteAllAsync(
      data->buffer.data(), size, base::Bind(&PerformRead, base::Passed(&data)),
      base::Bind(&IgnoreStreamError), nullptr);
}

// Reads the data from the source stream into a buffer and invokes PerformWrite
// when done.
void PerformRead(std::unique_ptr<StreamCopyData> data) {
  data->src_stream->ReadAsync(data->buffer.data(), data->buffer.size(),
                              base::Bind(&PerformWrite, base::Passed(&data)),
                              base::Bind(&IgnoreStreamError), nullptr);
}

// Copies the data from |src_stream| to the destination stream represented
// by a file descriptor |fd|.
void WriteResponseData(chromeos::StreamPtr src_stream,
                       const dbus::FileDescriptor& fd) {
  std::unique_ptr<StreamCopyData> data{new StreamCopyData};
  int dupfd = dup(fd.value());
  data->src_stream = std::move(src_stream);
  data->dest_stream =
      chromeos::FileStream::FromFileDescriptor(dupfd, true, nullptr);
  data->buffer.resize(4096);  // Read buffer of 4 KiB.
  CHECK(data->src_stream);
  CHECK(data->dest_stream);
  PerformRead(std::move(data));
}

}  // anonymous namespace

const char ProtocolHandler::kHttp[] = "http";
const char ProtocolHandler::kHttps[] = "https";

ProtocolHandler::ProtocolHandler(const std::string& name, Server* server)
    : name_{name}, server_{server} {
}

ProtocolHandler::~ProtocolHandler() {
  // Remove any existing handlers, so the web server knows that we don't
  // need them anymore.

  // We need to get a copy of the map keys since removing the handlers will
  // modify the map in the middle of the loop and that's not a good thing.
  auto handler_ids = chromeos::GetMapKeys(request_handlers_);
  for (int handler_id : handler_ids) {
    RemoveHandler(handler_id);
  }
}

std::string ProtocolHandler::GetName() const {
  return name_;
}

std::set<uint16_t> ProtocolHandler::GetPorts() const {
  std::set<uint16_t> ports;
  for (const auto& pair : proxies_)
    ports.insert(pair.second->port());
  return ports;
}

std::set<std::string> ProtocolHandler::GetProtocols() const {
  std::set<std::string> protocols;
  for (const auto& pair : proxies_)
    protocols.insert(pair.second->protocol());
  return protocols;
}

chromeos::Blob ProtocolHandler::GetCertificateFingerprint() const {
  chromeos::Blob fingerprint;
  for (const auto& pair : proxies_) {
    fingerprint = pair.second->certificate_fingerprint();
    if (!fingerprint.empty())
      break;
  }
  return fingerprint;
}

int ProtocolHandler::AddHandler(
    const std::string& url,
    const std::string& method,
    std::unique_ptr<RequestHandlerInterface> handler) {
  request_handlers_.emplace(
      ++last_handler_id_,
      HandlerMapEntry{url, method,
                      std::map<ProtocolHandlerProxy*, std::string>{},
                      std::move(handler)});
  // For each instance of remote protocol handler object sharing the same name,
  // add the request handler.
  for (const auto& pair : proxies_) {
    pair.second->AddRequestHandlerAsync(
        url,
        method,
        server_->service_name_,
        base::Bind(&ProtocolHandler::AddHandlerSuccess,
                   weak_ptr_factory_.GetWeakPtr(),
                   last_handler_id_,
                   pair.second),
        base::Bind(&ProtocolHandler::AddHandlerError,
                   weak_ptr_factory_.GetWeakPtr(),
                   last_handler_id_));
  }
  return last_handler_id_;
}

int ProtocolHandler::AddHandlerCallback(
    const std::string& url,
    const std::string& method,
    const base::Callback<RequestHandlerInterface::HandlerSignature>&
        handler_callback) {
  std::unique_ptr<RequestHandlerInterface> handler{
      new RequestHandlerCallback{handler_callback}};
  return AddHandler(url, method, std::move(handler));
}

bool ProtocolHandler::RemoveHandler(int handler_id) {
  auto p = request_handlers_.find(handler_id);
  if (p == request_handlers_.end())
    return false;

  for (const auto& pair : p->second.remote_handler_ids) {
    pair.first->RemoveRequestHandlerAsync(
        pair.second,
        base::Bind(&base::DoNothing),
        base::Bind(&IgnoreDBusError));
  }

  request_handlers_.erase(p);
  return true;
}

void ProtocolHandler::Connect(ProtocolHandlerProxy* proxy) {
  proxies_.emplace(proxy->GetObjectPath(), proxy);
  for (const auto& pair : request_handlers_) {
    proxy->AddRequestHandlerAsync(
        pair.second.url,
        pair.second.method,
        server_->service_name_,
        base::Bind(&ProtocolHandler::AddHandlerSuccess,
                   weak_ptr_factory_.GetWeakPtr(),
                   pair.first,
                   proxy),
        base::Bind(&ProtocolHandler::AddHandlerError,
                   weak_ptr_factory_.GetWeakPtr(),
                   pair.first));
  }
}

void ProtocolHandler::Disconnect(const dbus::ObjectPath& object_path) {
  proxies_.erase(object_path);
  if (proxies_.empty())
    remote_handler_id_map_.clear();
  for (auto& pair : request_handlers_)
    pair.second.remote_handler_ids.clear();
}

void ProtocolHandler::AddHandlerSuccess(int handler_id,
                                        ProtocolHandlerProxy* proxy,
                                        const std::string& remote_handler_id) {
  auto p = request_handlers_.find(handler_id);
  CHECK(p != request_handlers_.end());
  p->second.remote_handler_ids.emplace(proxy, remote_handler_id);

  remote_handler_id_map_.emplace(remote_handler_id, handler_id);
}

void ProtocolHandler::AddHandlerError(int handler_id, chromeos::Error* error) {
  // Nothing to do at the moment.
}

bool ProtocolHandler::ProcessRequest(const std::string& protocol_handler_id,
                                     const std::string& remote_handler_id,
                                     const std::string& request_id,
                                     std::unique_ptr<Request> request,
                                     chromeos::ErrorPtr* error) {
  request_id_map_.emplace(request_id, protocol_handler_id);
  auto id_iter = remote_handler_id_map_.find(remote_handler_id);
  if (id_iter == remote_handler_id_map_.end()) {
    chromeos::Error::AddToPrintf(error, FROM_HERE,
                                 chromeos::errors::dbus::kDomain,
                                 DBUS_ERROR_FAILED,
                                 "Unknown request handler '%s'",
                                 remote_handler_id.c_str());
    return false;
  }
  auto handler_iter = request_handlers_.find(id_iter->second);
  if (handler_iter == request_handlers_.end()) {
    chromeos::Error::AddToPrintf(error, FROM_HERE,
                                 chromeos::errors::dbus::kDomain,
                                 DBUS_ERROR_FAILED,
                                 "Handler # %d is no longer available",
                                 id_iter->second);
    return false;
  }
  handler_iter->second.handler->HandleRequest(
      std::move(request),
      std::unique_ptr<Response>{new Response{this, request_id}});
  return true;
}

void ProtocolHandler::CompleteRequest(
    const std::string& request_id,
    int status_code,
    const std::multimap<std::string, std::string>& headers,
    chromeos::StreamPtr data_stream) {
  ProtocolHandlerProxy* proxy = GetRequestProtocolHandlerProxy(request_id);
  if (!proxy)
    return;

  std::vector<std::tuple<std::string, std::string>> header_list;
  header_list.reserve(headers.size());
  for (const auto& pair : headers)
    header_list.emplace_back(pair.first, pair.second);

  int64_t data_size = -1;
  if (data_stream->CanGetSize())
    data_size = data_stream->GetRemainingSize();
  proxy->CompleteRequestAsync(
      request_id, status_code, header_list, data_size,
      base::Bind(&WriteResponseData, base::Passed(&data_stream)),
      base::Bind(&IgnoreDBusError));
}

void ProtocolHandler::GetFileData(
    const std::string& request_id,
    int file_id,
    const base::Callback<void(chromeos::StreamPtr)>& success_callback,
    const base::Callback<void(chromeos::Error*)>& error_callback) {
  ProtocolHandlerProxy* proxy = GetRequestProtocolHandlerProxy(request_id);
  CHECK(proxy);

  // Store the success/error callback in a shared object so it can be referenced
  // by the two wrapper callbacks. Since the original callbacks MAY contain
  // move-only types, copying the base::Callback object is generally unsafe and
  // may destroy the source object of the copy (despite the fact that it is
  // constant). So, here we move both callbacks to |Callbacks| structure and
  // use a shared pointer to it in both success and error callback wrappers.
  struct Callbacks {
    base::Callback<void(chromeos::StreamPtr)> on_success;
    base::Callback<void(chromeos::Error*)> on_error;
  };
  auto callbacks = std::make_shared<Callbacks>();
  callbacks->on_success = success_callback;
  callbacks->on_error = error_callback;

  auto on_success = [callbacks](const dbus::FileDescriptor& fd) {
    chromeos::ErrorPtr error;
    // Unfortunately there is no way to take ownership of the file descriptor
    // since |fd| is a const reference, so duplicate the descriptor.
    int dupfd = dup(fd.value());
    auto stream = chromeos::FileStream::FromFileDescriptor(dupfd, true, &error);
    if (!stream)
      return callbacks->on_error.Run(error.get());
    callbacks->on_success.Run(std::move(stream));
  };
  auto on_error = [callbacks](chromeos::Error* error) {
    callbacks->on_error.Run(error);
  };

  proxy->GetRequestFileDataAsync(request_id, file_id, base::Bind(on_success),
                                 base::Bind(on_error));
}

ProtocolHandler::ProtocolHandlerProxy*
ProtocolHandler::GetRequestProtocolHandlerProxy(
    const std::string& request_id) const {
  auto iter = request_id_map_.find(request_id);
  if (iter == request_id_map_.end()) {
    LOG(ERROR) << "Can't find pending request with ID " << request_id;
    return nullptr;
  }
  std::string handler_id = iter->second;
  auto find_proxy_by_id = [handler_id](decltype(*proxies_.begin()) pair) {
    return pair.second->id() == handler_id;
  };
  auto proxy_iter = std::find_if(proxies_.begin(), proxies_.end(),
                                 find_proxy_by_id);
  if (proxy_iter == proxies_.end()) {
    LOG(WARNING) << "Completing a request after the handler proxy is removed";
    return nullptr;
  }
  return proxy_iter->second;
}


}  // namespace libwebserv
