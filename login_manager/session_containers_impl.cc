// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_containers_impl.h"

#include <errno.h>
#include <stdint.h>
#include <sys/mount.h>

#include <memory>
#include <string>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/posix/safe_strerror.h>

#include <libcontainer/libcontainer.h>
#include <login_manager/container_config_parser.h>

namespace {
const char kContainerRunPath[] = "/run/containers";
}  // anonymous namespace

namespace login_manager {

SessionContainersImpl::SessionContainersImpl(
    const base::FilePath& containers_directory)
    : containers_directory_(containers_directory) {
}

SessionContainersImpl::~SessionContainersImpl() {
  KillAllContainers();
}

bool SessionContainersImpl::StartContainer(const std::string& name) {
  const base::FilePath named_path = containers_directory_.Append(name);

  LOG(INFO) << "Starting container " << name;
  if (container_map_.count(name)) {
    LOG(ERROR) << "Container " << name << " already running";
    return false;
  }

  std::string config_json_data;
  if (!base::ReadFileToString(named_path.Append("config.json"),
                              &config_json_data)) {
    LOG(ERROR) << "Fail to read config for " << name;
    return false;
  }

  std::string runtime_json_data;
  if (!base::ReadFileToString(named_path.Append("runtime.json"),
                              &runtime_json_data)) {
    LOG(ERROR) << "Fail to read runtime config for " << name;
    return false;
  }

  ContainerConfigPtr config(container_config_create(),
                            &container_config_destroy);
  if (!ParseContainerConfig(config_json_data, runtime_json_data, name,
                            named_path, &config)) {
    LOG(ERROR) << "Failed to parse container configuration for " << name;
    return false;
  }

  std::unique_ptr<container, decltype(&container_destroy)> new_container(
      container_new(name.c_str(), kContainerRunPath),
      container_destroy);
  if (!new_container) {
    LOG(ERROR) << "Failed to create the new container named " << name;
    return false;
  }

  int rc = container_start(new_container.get(), config.get());
  if (rc != 0) {
    LOG(ERROR) << "Failed to start container " << name << ": "
               << base::safe_strerror(rc);
    return false;
  }

  container_map_.insert(std::make_pair(name, std::move(new_container)));

  return true;
}

bool SessionContainersImpl::WaitForContainerToExit(const std::string& name) {
  LOG(INFO) << "Waiting for container " << name;
  auto iter = container_map_.find(name);
  if (iter == container_map_.end())
    return false;
  int rc = container_wait(iter->second.get());
  if (rc != 0) {
    LOG(ERROR) << "Failed to wait for container " << name << ": "
               << base::safe_strerror(rc);
    return false;
  }
  container_map_.erase(iter);
  return true;
}

bool SessionContainersImpl::KillContainer(const std::string& name) {
  LOG(INFO) << "Killing off container " << name;
  auto iter = container_map_.find(name);
  if (iter == container_map_.end())
    return false;
  int rc = container_kill(iter->second.get());
  container_map_.erase(iter);
  if (rc != 0) {
    LOG(ERROR) << "Failed to kill container " << name << ": "
               << base::safe_strerror(rc);
    return false;
  }
  return true;
}

bool SessionContainersImpl::KillAllContainers() {
  LOG(INFO) << "Killing off all containers";
  bool all_killed = true;
  for (auto it = container_map_.begin(); it != container_map_.end(); ++it) {
    LOG(INFO) << "Killing container " << it->first;
    int rc = container_kill(it->second.get());
    if (rc != 0) {
      LOG(ERROR) << "Failed to kill container " << it->first << ": "
                 << base::safe_strerror(rc);
      all_killed = false;
    }
  }
  container_map_.clear();
  return all_killed;
}

bool SessionContainersImpl::GetRootFsPath(const std::string& name,
                                          base::FilePath* path_out) const {
  auto iter = container_map_.find(name);
  if (iter == container_map_.end())
    return false;
  *path_out = base::FilePath(container_root(iter->second.get()));
  return true;
}

bool SessionContainersImpl::GetContainerPID(const std::string& name,
                                            pid_t* pid_out) const {
  auto iter = container_map_.find(name);
  if (iter == container_map_.end())
    return false;
  *pid_out = container_pid(iter->second.get());
  return true;
}

}  // namespace login_manager
