// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/utils.h"

#include <arpa/inet.h>
#include <map>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/bind_helpers.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <chromeos/errors/error_codes.h>

namespace weave {

namespace {

// Truncates a string if it is too long. Used for error reporting with really
// long JSON strings.
std::string LimitString(const std::string& text, size_t max_len) {
  if (text.size() <= max_len)
    return text;
  return text.substr(0, max_len - 3) + "...";
}

const size_t kMaxStrLen = 1700;  // Log messages are limited to 2000 chars.

}  // anonymous namespace

const char kErrorDomain[] = "weave";
const char kFileReadError[] = "file_read_error";
const char kInvalidCategoryError[] = "invalid_category";
const char kInvalidPackageError[] = "invalid_package";

std::unique_ptr<base::DictionaryValue> LoadJsonDict(
    const base::FilePath& json_file_path,
    chromeos::ErrorPtr* error) {
  std::string json_string;
  if (!base::ReadFileToString(json_file_path, &json_string)) {
    chromeos::errors::system::AddSystemError(error, FROM_HERE, errno);
    chromeos::Error::AddToPrintf(error, FROM_HERE, kErrorDomain, kFileReadError,
                                 "Failed to read file '%s'",
                                 json_file_path.value().c_str());
    return {};
  }
  return LoadJsonDict(json_string, error);
}

std::unique_ptr<base::DictionaryValue> LoadJsonDict(
    const std::string& json_string,
    chromeos::ErrorPtr* error) {
  std::unique_ptr<base::DictionaryValue> result;
  std::string error_message;
  auto value = base::JSONReader::ReadAndReturnError(
      json_string, base::JSON_PARSE_RFC, nullptr, &error_message);
  if (!value) {
    chromeos::Error::AddToPrintf(
        error, FROM_HERE, chromeos::errors::json::kDomain,
        chromeos::errors::json::kParseError,
        "Error parsing JSON string '%s' (%zu): %s",
        LimitString(json_string, kMaxStrLen).c_str(), json_string.size(),
        error_message.c_str());
    return result;
  }
  base::DictionaryValue* dict_value = nullptr;
  if (!value->GetAsDictionary(&dict_value)) {
    chromeos::Error::AddToPrintf(error, FROM_HERE,
                                 chromeos::errors::json::kDomain,
                                 chromeos::errors::json::kObjectExpected,
                                 "JSON string '%s' is not a JSON object",
                                 LimitString(json_string, kMaxStrLen).c_str());
    return result;
  } else {
    // |value| is now owned by |dict_value|.
    base::IgnoreResult(value.release());
  }
  result.reset(dict_value);
  return result;
}

int ConnectSocket(const std::string& host, uint16_t port) {
  std::string service = std::to_string(port);
  addrinfo hints = {0, AF_UNSPEC, SOCK_STREAM};
  addrinfo* result = nullptr;
  if (getaddrinfo(host.c_str(), service.c_str(), &hints, &result)) {
    PLOG(WARNING) << "Failed to resolve host name: " << host;
    return -1;
  }

  int socket_fd = -1;
  for (const addrinfo* info = result; info != nullptr; info = info->ai_next) {
    socket_fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (socket_fd < 0)
      continue;

    char str[INET6_ADDRSTRLEN] = {};
    inet_ntop(info->ai_family, info->ai_addr, str, info->ai_addrlen);
    LOG(INFO) << "Connecting to address: " << str;
    if (connect(socket_fd, info->ai_addr, info->ai_addrlen) == 0)
      break;  // Success.

    PLOG(WARNING) << "Failed to connect to address: " << str;
    close(socket_fd);
    socket_fd = -1;
  }

  freeaddrinfo(result);
  return socket_fd;
}

}  // namespace weave
