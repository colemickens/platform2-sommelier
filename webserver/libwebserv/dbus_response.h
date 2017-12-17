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

#ifndef WEBSERVER_LIBWEBSERV_DBUS_RESPONSE_H_
#define WEBSERVER_LIBWEBSERV_DBUS_RESPONSE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <brillo/streams/stream.h>
#include <libwebserv/response.h>

namespace libwebserv {

class DBusProtocolHandler;

// Implementation of the Response interface.
class DBusResponse final : public Response {
 public:
  ~DBusResponse() override;

  // Overrides from Response.
  void AddHeaders(
      const std::vector<std::pair<std::string, std::string>>& headers) override;
  void Reply(int status_code,
             brillo::StreamPtr data_stream,
             const std::string& mime_type) override;

 private:
  friend class DBusProtocolHandler;

  LIBWEBSERV_PRIVATE DBusResponse(DBusProtocolHandler* handler,
                                  const std::string& request_id);

  DBusProtocolHandler* handler_{nullptr};
  std::string request_id_;
  int status_code_{0};
  brillo::StreamPtr data_stream_;
  std::multimap<std::string, std::string> headers_;
  bool reply_sent_{false};

  DISALLOW_COPY_AND_ASSIGN(DBusResponse);
};

}  // namespace libwebserv

#endif  // WEBSERVER_LIBWEBSERV_DBUS_RESPONSE_H_
