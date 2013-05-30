// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "server/peer_update_manager.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include <base/logging.h>
#include <base/bind.h>

using std::string;

using base::FilePath;

namespace p2p {

namespace server {

static size_t GetFileSize(const FilePath& file_path) {
  struct stat statbuf;
  if (stat(file_path.value().c_str(), &statbuf) != 0) {
    LOG(ERROR) << "Error getting file size for " << file_path.value() << ": "
               << strerror(errno);
    return 0;
  }
  return statbuf.st_size;
}

PeerUpdateManager::PeerUpdateManager(FileWatcher* file_watcher,
                                     ServicePublisher* publisher,
                                     HttpServer* http_server)
    : file_watcher_(file_watcher),
      publisher_(publisher),
      http_server_(http_server),
      num_connections_(0) {}

PeerUpdateManager::~PeerUpdateManager() {}

void PeerUpdateManager::Publish(const FilePath& file) {
  if (file.Extension() == file_watcher_->file_extension()) {
    string id_with_extension = file.BaseName().value();
    string id = id_with_extension.substr(0, id_with_extension.size() - 4);
    size_t file_size = GetFileSize(file);
    publisher_->AddFile(id, file_size);
    UpdateHttpServer();
  }
}

void PeerUpdateManager::Unpublish(const FilePath& file) {
  if (file.Extension() == file_watcher_->file_extension()) {
    string id_with_extension = file.BaseName().value();
    string id = id_with_extension.substr(0, id_with_extension.size() - 4);
    publisher_->RemoveFile(id);
    UpdateHttpServer();
  }
}

void PeerUpdateManager::Update(const FilePath& file) {
  if (file.Extension() == file_watcher_->file_extension()) {
    string id_with_extension = file.BaseName().value();
    string id = id_with_extension.substr(0, id_with_extension.size() - 4);
    size_t file_size = GetFileSize(file);
    publisher_->UpdateFileSize(id, file_size);
  }
}

void PeerUpdateManager::UpdateHttpServer() {
  int num_files = publisher_->files().size();
  if (num_files > 0) {
    if (!http_server_->IsRunning()) {
      http_server_->Start();
    }
  } else {
    if (http_server_->IsRunning()) {
      http_server_->Stop();
      UpdateNumConnections(0);
    }
  }
}

void PeerUpdateManager::UpdateNumConnections(int num_connections) {
  if (num_connections_ != num_connections) {
    num_connections_ = num_connections;
    publisher_->SetNumConnections(num_connections);
  }
}

void PeerUpdateManager::OnFileWatcherChanged(
    const FilePath& file,
    FileWatcher::EventType event_type) {
  VLOG(2) << "FileWatcher changed, path=" << file.value()
          << ", event_type=" << event_type;

  switch (event_type) {
    case FileWatcher::EventType::kFileAdded:
      Publish(file);
      break;

    case FileWatcher::EventType::kFileRemoved:
      Unpublish(file);
      break;

    case FileWatcher::EventType::kFileChanged:
      Update(file);
      break;
  }
}

void PeerUpdateManager::OnHttpServerNumConnectionsChanged(int num_connections) {
  UpdateNumConnections(num_connections);
}

void PeerUpdateManager::Init() {
  http_server_->SetNumConnectionsCallback(
      base::Bind(&PeerUpdateManager::OnHttpServerNumConnectionsChanged,
                 base::Unretained(this)));

  for (auto const& file : file_watcher_->files()) {
    Publish(file);
  }

  // TODO(zeuthen): Move to AddChangedCallback() for multiple
  // listeners. Or delegate pattern?
  file_watcher_->SetChangedCallback(base::Bind(
      &PeerUpdateManager::OnFileWatcherChanged, base::Unretained(this)));
}

}  // namespace server

}  // namespace p2p
