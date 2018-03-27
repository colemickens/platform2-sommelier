// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>
#include <base/threading/thread_task_runner_handle.h>
#include <base/time/time.h>

#include "vm_tools/common/constants.h"
#include "vm_tools/garcon/desktop_file.h"
#include "vm_tools/garcon/host_notifier.h"

namespace {

constexpr char kHostIpFile[] = "/dev/.host_ip";
constexpr char kSecurityTokenFile[] = "/dev/.container_token";
constexpr int kSecurityTokenLength = 36;
// File extension for desktop files.
constexpr char kDesktopFileExtension[] = ".desktop";
// Duration over which we coalesce changes to the desktop file system.
constexpr base::TimeDelta kFilesystemChangeCoalesceTime =
    base::TimeDelta::FromSeconds(5);

std::string GetHostIp() {
  char host_addr[INET_ADDRSTRLEN + 1];
  base::FilePath host_ip_path(kHostIpFile);
  int num_read = base::ReadFile(host_ip_path, host_addr, sizeof(host_addr) - 1);
  if (num_read <= 0) {
    LOG(ERROR) << "Failed reading the host IP from: "
               << host_ip_path.MaybeAsASCII();
    return "";
  }
  host_addr[num_read] = '\0';
  return std::string(host_addr);
}

std::string GetSecurityToken() {
  char token[kSecurityTokenLength + 1];
  base::FilePath security_token_path(kSecurityTokenFile);
  int num_read = base::ReadFile(security_token_path, token, sizeof(token) - 1);
  if (num_read <= 0) {
    LOG(ERROR) << "Failed reading the container token from: "
               << security_token_path.MaybeAsASCII();
    return "";
  }
  token[num_read] = '\0';
  return std::string(token);
}

}  // namespace

namespace vm_tools {
namespace garcon {

// static
std::unique_ptr<HostNotifier> HostNotifier::Create() {
  auto notifier = base::WrapUnique(new HostNotifier());
  if (!notifier->Init()) {
    notifier.reset();
  }
  return notifier;
}

// static
bool HostNotifier::NotifyHostOfContainerShutdown() {
  // Notify the host system that we are ready.
  std::string host_ip = GetHostIp();
  std::string token = GetSecurityToken();
  if (token.empty() || host_ip.empty()) {
    return false;
  }
  vm_tools::container::ContainerListener::Stub stub(grpc::CreateChannel(
      base::StringPrintf("%s:%u", host_ip.c_str(), vm_tools::kGarconPort),
      grpc::InsecureChannelCredentials()));
  grpc::ClientContext ctx;
  vm_tools::container::ContainerShutdownInfo shutdown_info;
  shutdown_info.set_token(token);
  vm_tools::EmptyMessage empty;
  grpc::Status status = stub.ContainerShutdown(&ctx, shutdown_info, &empty);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to notify host system that container is shutting "
                 << "down: " << status.error_message();
    return false;
  }
  return true;
}

HostNotifier::HostNotifier() : weak_ptr_factory_(this) {}

bool HostNotifier::Init() {
  std::string host_ip = GetHostIp();
  token_ = GetSecurityToken();
  if (token_.empty() || host_ip.empty()) {
    return false;
  }
  stub_ = std::make_unique<vm_tools::container::ContainerListener::Stub>(
      grpc::CreateChannel(
          base::StringPrintf("%s:%u", host_ip.c_str(), vm_tools::kGarconPort),
          grpc::InsecureChannelCredentials()));
  if (!NotifyHostGarconIsReady()) {
    return false;
  }

  // Setup all of our watchers for changes to any of the paths where .desktop
  // files may reside.
  std::vector<base::FilePath> watch_paths =
      DesktopFile::GetPathsForDesktopFiles();
  for (auto& path : watch_paths) {
    std::unique_ptr<base::FilePathWatcher> watcher =
        std::make_unique<base::FilePathWatcher>();
    if (!watcher->Watch(path, true,
                        base::Bind(&HostNotifier::DesktopPathsChanged,
                                   weak_ptr_factory_.GetWeakPtr()))) {
      LOG(ERROR) << "Failed setting up filesystem path watcher for dir: "
                 << path.value();
      return false;
    }
    watchers_.emplace_back(std::move(watcher));
  }

  if (!SendAppListToHost()) {
    return false;
  }

  return true;
}

bool HostNotifier::NotifyHostGarconIsReady() {
  // Notify the host system that we are ready.
  grpc::ClientContext ctx;
  vm_tools::container::ContainerStartupInfo startup_info;
  startup_info.set_token(token_);
  vm_tools::EmptyMessage empty;
  grpc::Status status = stub_->ContainerReady(&ctx, startup_info, &empty);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to notify host system that container is ready: "
                 << status.error_message();
    return false;
  }
  return true;
}

bool HostNotifier::SendAppListToHost() {
  // Generate the protobufs for the list of all the installed applications and
  // make the gRPC call to the host to update them.

  vm_tools::container::UpdateApplicationListRequest request;
  request.set_token(token_);
  vm_tools::EmptyMessage empty;

  // If we hit duplicate IDs, then we are supposed to use the first one only.
  std::set<std::string> unique_app_ids;

  // Get the list of directories that we should search for .desktop files
  // recursively and then perform the search.
  std::vector<base::FilePath> search_paths =
      DesktopFile::GetPathsForDesktopFiles();
  for (auto curr_path : search_paths) {
    base::FileEnumerator file_enum(curr_path, true,
                                   base::FileEnumerator::FILES);
    for (base::FilePath enum_path = file_enum.Next(); !enum_path.empty();
         enum_path = file_enum.Next()) {
      if (enum_path.FinalExtension() != kDesktopFileExtension) {
        continue;
      }
      // We have a .desktop file path, parse it and then add it to the
      // protobuf if it parses successfully.
      std::unique_ptr<DesktopFile> desktop_file =
          DesktopFile::ParseDesktopFile(enum_path);
      if (!desktop_file) {
        LOG(WARNING) << "Failed parsing the .desktop file: "
                     << enum_path.value();
        continue;
      }
      // Make sure this .desktop file is one we should send to the host.
      // There are various cases where we do not want to transmit certain
      // .desktop files.
      if (!desktop_file->ShouldPassToHost() ||
          !unique_app_ids.insert(desktop_file->app_id()).second) {
        continue;
      }
      // Add this app to the list in the protobuf and populate all of its
      // fields.
      vm_tools::container::Application* app = request.add_application();
      app->set_desktop_file_id(desktop_file->app_id());
      const std::map<std::string, std::string>& name_map =
          desktop_file->locale_name_map();
      vm_tools::container::Application::LocalizedString* names =
          app->mutable_name();
      for (const auto& name_entry : name_map) {
        vm_tools::container::Application::LocalizedString::StringWithLocale*
            locale_string = names->add_values();
        locale_string->set_locale(name_entry.first);
        locale_string->set_value(name_entry.second);
      }
      const std::map<std::string, std::string>& comment_map =
          desktop_file->locale_comment_map();
      vm_tools::container::Application::LocalizedString* comments =
          app->mutable_comment();
      for (const auto& comment_entry : comment_map) {
        vm_tools::container::Application::LocalizedString::StringWithLocale*
            locale_string = comments->add_values();
        locale_string->set_locale(comment_entry.first);
        locale_string->set_value(comment_entry.second);
      }
      for (const auto& mime_type : desktop_file->mime_types()) {
        app->add_mime_types(mime_type);
      }
      app->set_no_display(desktop_file->no_display());
      app->set_startup_wm_class(desktop_file->startup_wm_class());
    }
  }

  // Clear this in case it was set, this all happens on the same thread.
  update_app_list_posted_ = false;

  // Now make the gRPC call to send this list to the host.
  grpc::ClientContext ctx;
  grpc::Status status = stub_->UpdateApplicationList(&ctx, request, &empty);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to notify host of the application list: "
                 << status.error_message();
    return false;
  }
  return true;
}

void HostNotifier::SendAppListToHostNoStatus() {
  SendAppListToHost();
}

void HostNotifier::DesktopPathsChanged(const base::FilePath& path, bool error) {
  if (error) {
    // TODO(jkardatzke): Determine best how to handle errors here. We may want
    // to restart this specific watcher in that case.
    LOG(ERROR) << "Error detected in file path watching for path: "
               << path.value();
    return;
  }

  // We don't want to trigger an update every time there's a change, instead
  // wait a bit and coalesce potential groups of changes that may occur. We
  // don't want to wait too long though because then the user may feel that it
  // is unresponsive in newly installed applications not showing up in the
  // launcher when they check.
  if (update_app_list_posted_) {
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&HostNotifier::SendAppListToHostNoStatus,
                 weak_ptr_factory_.GetWeakPtr()),
      kFilesystemChangeCoalesceTime);
  update_app_list_posted_ = true;
}

}  // namespace garcon
}  // namespace vm_tools
