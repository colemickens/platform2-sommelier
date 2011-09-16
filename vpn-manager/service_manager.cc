// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpn-manager/service_manager.h"

#include <arpa/inet.h>  // for inet_ntop and inet_pton
#include <netdb.h>  // for getaddrinfo

#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/string_split.h"
#include "base/string_util.h"

const FilePath* ServiceManager::temp_path_ = NULL;
const char* ServiceManager::temp_base_path_ = "/home/chronos/user/tmp";

ServiceManager::ServiceManager(const std::string& service_name)
    : is_running_(false),
      was_stopped_(false),
      debug_(false),
      inner_service_(NULL),
      outer_service_(NULL),
      service_name_(service_name),
      error_(kServiceErrorNoError) {
}

ServiceManager::~ServiceManager() {
}

void ServiceManager::OnStarted() {
  CHECK(!is_running_ && !was_stopped_);
  CHECK(outer_service_ == NULL || outer_service_->is_running_);
  is_running_ = true;
  if (inner_service_ == NULL)
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
  CHECK(inner_service_ == NULL || !inner_service_->is_running_);
  is_running_ = false;
  was_stopped_ = true;
  if (outer_service_ != NULL) {
    outer_service_->Stop();
  }
}

void ServiceManager::OnSyslogOutput(const std::string& prefix,
                                    const std::string& line) {
}

void ServiceManager::RegisterError(ServiceError error) {
  // Register a given error only if it has a higher value than the one
  // currently registered as error identifiers are ordered from less
  // specific to more specific.
  if (error_ < error)
    error_ = error;
}

ServiceError ServiceManager::GetError() const {
  if (inner_service_ != NULL) {
    ServiceError inner_service_error = inner_service_->GetError();
    if (inner_service_error != kServiceErrorNoError)
      return inner_service_error;
  }
  return error_;
}

void ServiceManager::InitializeDirectories(ScopedTempDir* scoped_temp_path) {
  bool success =
      scoped_temp_path->CreateUniqueTempDirUnderPath(FilePath(temp_base_path_));
  temp_path_ = &scoped_temp_path->path();
  PLOG_IF(INFO, !success) << "Could not create temporary directory. ";
  LOG_IF(INFO, success) << "Using temporary directory " << temp_path_->value();
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
  std::vector<std::string> lines;
  base::SplitString(*partial_line, '\n', &lines);
  if (lines.empty()) {
    partial_line->clear();
  } else {
    *partial_line = lines.back();
    lines.pop_back();
  }
  for (size_t i = 0; i < lines.size(); ++i) {
    LOG(INFO) << prefix << lines[i];
    OnSyslogOutput(prefix, lines[i]);
  }
}

bool ServiceManager::ResolveNameToSockAddr(const std::string& name,
                                           struct sockaddr* address) {
  struct addrinfo *address_info;
  int s = getaddrinfo(name.c_str(), NULL, NULL, &address_info);
  if (s != 0) {
    LOG(ERROR) << "getaddrinfo failed: " << gai_strerror(s);
    return false;
  }
  *address = *address_info->ai_addr;
  freeaddrinfo(address_info);
  return true;
}

bool ServiceManager::ConvertIPStringToSockAddr(
    const std::string& address_text,
    struct sockaddr* address) {
  struct addrinfo hint_info = {};
  struct addrinfo *address_info;
  hint_info.ai_flags = AI_NUMERICHOST;
  int s = getaddrinfo(address_text.c_str(), NULL, &hint_info, &address_info);
  if (s != 0) {
    LOG(ERROR) << "getaddrinfo failed: " << gai_strerror(s);
    return false;
  }
  *address = *address_info->ai_addr;
  freeaddrinfo(address_info);
  return true;
}

bool ServiceManager::ConvertSockAddrToIPString(
    const struct sockaddr& address,
    std::string* address_text) {
  char str[INET6_ADDRSTRLEN] = { 0 };
  switch (address.sa_family) {
    case AF_INET:
      if (!inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in*>(
              &address)->sin_addr, str, INET6_ADDRSTRLEN)) {
        PLOG(ERROR) << "inet_ntop failed";
        return false;
      }
      break;
    case AF_INET6:
      if (!inet_ntop(AF_INET6, &reinterpret_cast<const sockaddr_in6*>(
              &address)->sin6_addr, str, INET6_ADDRSTRLEN)) {
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
    const struct sockaddr& remote_address,
    struct sockaddr* local_address) {
  int sock = HANDLE_EINTR(socket(AF_INET, SOCK_DGRAM, 0));
  if (sock < 0) {
    LOG(ERROR) << "Unable to create socket";
    return false;
  }
  if (HANDLE_EINTR(connect(sock, &remote_address,
                           sizeof(struct sockaddr))) != 0) {
    LOG(ERROR) << "Unable to connect";
    HANDLE_EINTR(close(sock));
    return false;
  }
  bool result = false;
  socklen_t addr_len = sizeof(*local_address);
  if (getsockname(sock, local_address, &addr_len) != 0) {
    PLOG(ERROR) << "getsockname failed on socket";
    goto error_label;
  }
  result = true;

 error_label:
  HANDLE_EINTR(close(sock));
  return result;
}
