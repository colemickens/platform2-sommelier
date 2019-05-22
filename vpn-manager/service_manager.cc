// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpn-manager/service_manager.h"

#include <vector>

#include <arpa/inet.h>  // for inet_ntop and inet_pton
#include <netdb.h>      // for getaddrinfo

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

using base::FilePath;

namespace vpn_manager {

ServiceManager::ServiceManager(const std::string& service_name,
                               const base::FilePath& temp_path)
    : is_running_(false),
      was_stopped_(false),
      inner_service_(nullptr),
      outer_service_(nullptr),
      service_name_(service_name),
      error_(kServiceErrorNoError),
      temp_path_(temp_path) {}

void ServiceManager::OnStarted() {
  CHECK(!is_running_ && !was_stopped_);
  CHECK(outer_service_ == nullptr || outer_service_->is_running_);
  is_running_ = true;
  if (inner_service_ == nullptr)
    return;

  DLOG(INFO) << "Starting inner " << inner_service_->service_name();
  if (!inner_service_->Start()) {
    // Inner service could not be started, stop this layer.
    LOG(ERROR) << "Inner service " << inner_service_->service_name()
               << " failed.  Stopping " << service_name();
    Stop();
  }
}

void ServiceManager::OnStopped(bool was_error) {
  CHECK(inner_service_ == nullptr || !inner_service_->is_running_);
  is_running_ = false;
  was_stopped_ = true;
  if (outer_service_ != nullptr) {
    outer_service_->Stop();
  }
}

void ServiceManager::OnSyslogOutput(const std::string& prefix,
                                    const std::string& line) {}

void ServiceManager::RegisterError(ServiceError error) {
  // Register a given error only if it has a higher value than the one
  // currently registered as error identifiers are ordered from less
  // specific to more specific.
  if (error_ < error)
    error_ = error;
}

ServiceError ServiceManager::GetError() const {
  if (inner_service_ != nullptr) {
    ServiceError inner_service_error = inner_service_->GetError();
    if (inner_service_error != kServiceErrorNoError)
      return inner_service_error;
  }
  return error_;
}

void ServiceManager::WriteFdToSyslog(int fd,
                                     const std::string& prefix,
                                     std::string* partial_line) {
  char buffer[256];
  ssize_t written = HANDLE_EINTR(read(fd, &buffer, sizeof(buffer) - 1));
  if (written < 0) {
    PLOG(WARNING) << "Error condition on " << prefix << " pipe";
    return;
  }
  buffer[written] = '\0';
  partial_line->append(buffer);
  std::vector<std::string> lines = base::SplitString(
      *partial_line, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (lines.empty()) {
    partial_line->clear();
  } else {
    *partial_line = lines.back();
    lines.pop_back();
  }
  for (const auto& line : lines) {
    LOG(INFO) << prefix << line;
    OnSyslogOutput(prefix, line);
  }
}

bool ServiceManager::ResolveNameToSockAddr(const std::string& name,
                                           struct sockaddr* address) {
  struct addrinfo* address_info;
  int s = getaddrinfo(name.c_str(), nullptr, nullptr, &address_info);
  if (s != 0) {
    LOG(ERROR) << "getaddrinfo failed: " << gai_strerror(s);
    return false;
  }
  *address = *address_info->ai_addr;
  freeaddrinfo(address_info);
  return true;
}

bool ServiceManager::ConvertIPStringToSockAddr(const std::string& address_text,
                                               struct sockaddr* address) {
  struct addrinfo hint_info = {};
  struct addrinfo* address_info;
  hint_info.ai_flags = AI_NUMERICHOST;
  int s = getaddrinfo(address_text.c_str(), nullptr, &hint_info, &address_info);
  if (s != 0) {
    LOG(ERROR) << "getaddrinfo failed: " << gai_strerror(s);
    return false;
  }
  *address = *address_info->ai_addr;
  freeaddrinfo(address_info);
  return true;
}

bool ServiceManager::ConvertSockAddrToIPString(const struct sockaddr& address,
                                               std::string* address_text) {
  char str[INET6_ADDRSTRLEN] = {0};
  switch (address.sa_family) {
    case AF_INET:
      if (!inet_ntop(
              AF_INET,
              &(reinterpret_cast<const sockaddr_in*>(&address)->sin_addr), str,
              sizeof(str))) {
        PLOG(ERROR) << "inet_ntop failed";
        return false;
      }
      break;
    case AF_INET6:
      if (!inet_ntop(
              AF_INET6,
              &(reinterpret_cast<const sockaddr_in6*>(&address)->sin6_addr),
              str, sizeof(str))) {
        PLOG(ERROR) << "inet_ntop failed";
        return false;
      }
      break;
    default:
      LOG(ERROR) << "Unknown address family";
      return false;
  }
  *address_text = str;
  return true;
}

bool ServiceManager::GetLocalAddressFromRemote(
    const struct sockaddr& remote_address, struct sockaddr* local_address) {
  base::ScopedFD socket_fd(socket(AF_INET, SOCK_DGRAM, 0));
  if (!socket_fd.is_valid()) {
    PLOG(ERROR) << "Unable to create socket";
    return false;
  }
  if (HANDLE_EINTR(connect(socket_fd.get(), &remote_address,
                           sizeof(struct sockaddr))) != 0) {
    PLOG(ERROR) << "Unable to connect";
    return false;
  }
  socklen_t addr_len = sizeof(*local_address);
  if (getsockname(socket_fd.get(), local_address, &addr_len) != 0) {
    PLOG(ERROR) << "getsockname failed on socket";
    return false;
  }
  return true;
}

}  // namespace vpn_manager
