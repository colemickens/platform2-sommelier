// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_SERVER_FAKE_SERVICE_PUBLISHER_H__
#define P2P_SERVER_FAKE_SERVICE_PUBLISHER_H__

#include <map>
#include <string>

#include "p2p/server/service_publisher.h"

namespace p2p {

namespace server {

// A ServicePublisher that doesn't really do anything.
class FakeServicePublisher : public ServicePublisher {
 public:
  FakeServicePublisher() : num_connections_(0) {}

  virtual void AddFile(const std::string& file, size_t file_size) {
    files_[file] = file_size;
  }

  virtual void RemoveFile(const std::string& file) {
    std::map<std::string, size_t>::iterator it = files_.find(file);
    if (it != files_.end())
      files_.erase(it);
  }

  virtual void UpdateFileSize(const std::string& file, size_t file_size) {
    files_[file] = file_size;
  }

  virtual void SetNumConnections(int num_connections) {
    num_connections_ = num_connections;
  }

  virtual std::map<std::string, size_t> files() { return files_; }

 private:
  std::map<std::string, size_t> files_;
  int num_connections_;

  DISALLOW_COPY_AND_ASSIGN(FakeServicePublisher);
};

}  // namespace server

}  // namespace p2p

#endif  // P2P_SERVER_FAKE_SERVICE_PUBLISHER_H__
