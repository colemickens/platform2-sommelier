//
// Copyright (C) 2012 The Android Open Source Project
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
//

#include "shill/dbus_manager.h"

#include <base/bind.h>

#include "shill/control_interface.h"
#include "shill/dbus_service_proxy_interface.h"
#include "shill/error.h"
#include "shill/logging.h"

using base::Bind;
using base::Unretained;
using std::list;
using std::map;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(DBusManager* d) { return "(dbus_manager)"; }
}

namespace {

const int kDefaultRPCTimeoutMS = 30000;

}  // namespace

DBusManager::DBusManager(ControlInterface* control_interface)
    : control_interface_(control_interface) {}

DBusManager::~DBusManager() {}

void DBusManager::Start() {
  SLOG(this, 2) << __func__;
  if (proxy_.get()) {
    return;
  }
  proxy_.reset(control_interface_->CreateDBusServiceProxy());
  proxy_->set_name_owner_changed_callback(
      Bind(&DBusManager::OnNameOwnerChanged, Unretained(this)));
}

void DBusManager::Stop() {
  SLOG(this, 2) << __func__;
  proxy_.reset();
  name_watchers_.clear();
}

DBusNameWatcher* DBusManager::CreateNameWatcher(
    const string& name,
    const DBusNameWatcher::NameAppearedCallback& name_appeared_callback,
    const DBusNameWatcher::NameVanishedCallback& name_vanished_callback) {
  // DBusNameWatcher holds a weak pointer to, and thus may outlive, this
  // DBusManager object.
  std::unique_ptr<DBusNameWatcher> name_watcher(new DBusNameWatcher(
      this, name, name_appeared_callback, name_vanished_callback));
  name_watchers_[name].push_back(name_watcher.get());

  Error error;
  proxy_->GetNameOwner(name,
                       &error,
                       Bind(&DBusManager::OnGetNameOwnerComplete,
                            AsWeakPtr(),
                            name_watcher->AsWeakPtr()),
                       kDefaultRPCTimeoutMS);
  // Ensures that the watcher gets an initial appear/vanish notification
  // regardless of the outcome of the GetNameOwner call.
  if (error.IsFailure()) {
    OnGetNameOwnerComplete(name_watcher->AsWeakPtr(), string(), error);
  }
  return name_watcher.release();
}

void DBusManager::RemoveNameWatcher(DBusNameWatcher* name_watcher) {
  CHECK(name_watcher);

  auto watcher_iterator = name_watchers_.find(name_watcher->name());
  if (watcher_iterator != name_watchers_.end()) {
    watcher_iterator->second.remove(name_watcher);
  }
}

void DBusManager::OnNameOwnerChanged(
    const string& name, const string& old_owner, const string& new_owner) {
  auto watcher_iterator = name_watchers_.find(name);
  if (watcher_iterator == name_watchers_.end()) {
    return;
  }
  LOG(INFO) << "DBus name '" << name << "' owner changed ('" << old_owner
            << "' -> '" << new_owner << "')";
  for (const auto& watcher : watcher_iterator->second) {
    watcher->OnNameOwnerChanged(new_owner);
  }
}

void DBusManager::OnGetNameOwnerComplete(
    const base::WeakPtr<DBusNameWatcher>& watcher,
    const string& unique_name,
    const Error& error) {
  if (watcher) {
    LOG(INFO) << "DBus name '" << watcher->name() << "' owner '" << unique_name
              << "' (" << error.message() << ")";
    watcher->OnNameOwnerChanged(error.IsSuccess() ? unique_name : string());
  }
}

}  // namespace shill
