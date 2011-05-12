// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpn-manager/service_manager.h"

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
      inner_service_(NULL),
      outer_service_(NULL),
      service_name_(service_name) {
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
    LOG(WARNING) << "Error condition on " << prefix << " pipe";
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
  }
}
