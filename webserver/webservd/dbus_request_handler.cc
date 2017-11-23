// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webservd/dbus_request_handler.h"

#include <tuple>
#include <vector>

#include <base/bind.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>

#include "libwebserv/dbus-proxies.h"
#include "webservd/request.h"
#include "webservd/server.h"

namespace webservd {

namespace {

void OnError(Request* request,
             bool debug,
             chromeos::Error* error) {
  std::string error_msg{"Internal Server Error"};
  if (debug) {
    error_msg += "\r\n" + error->GetMessage();
  }
  request->Complete(chromeos::http::status_code::InternalServerError, {},
                    chromeos::mime::text::kPlain, error_msg);
}

}  // anonymous namespace

DBusRequestHandler::DBusRequestHandler(Server* server,
                                       RequestHandlerProxy* handler_proxy)
    : server_{server},
      handler_proxy_{handler_proxy} {
}

void DBusRequestHandler::HandleRequest(Request* request) {
  std::vector<std::tuple<std::string, std::string>> headers;
  for (const auto& pair : request->GetHeaders())
    headers.emplace_back(pair.first, pair.second);

  std::vector<std::tuple<int32_t, std::string, std::string, std::string,
                         std::string>> files;
  int32_t index = 0;
  for (const auto& file : request->GetFileInfo()) {
    files.emplace_back(index++, file->field_name, file->file_name,
                        file->content_type, file->transfer_encoding);
  }

  std::vector<std::tuple<bool, std::string, std::string>> params;
  for (const auto& pair : request->GetDataGet())
    params.emplace_back(false, pair.first, pair.second);

  for (const auto& pair : request->GetDataPost())
    params.emplace_back(true, pair.first, pair.second);

  auto error_callback = base::Bind(&OnError,
                                   base::Unretained(request),
                                   server_->GetConfig().use_debug);

  auto request_id = std::make_tuple(request->GetProtocolHandlerID(),
                                    request->GetRequestHandlerID(),
                                    request->GetID(),
                                    request->GetURL(),
                                    request->GetMethod());

  handler_proxy_->ProcessRequestAsync(request_id,
                                      headers,
                                      params,
                                      files,
                                      request->GetBody(),
                                      base::Bind(&base::DoNothing),
                                      error_callback);
}

}  // namespace webservd
