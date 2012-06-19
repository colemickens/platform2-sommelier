// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_manager.h"

#include <base/bind.h>

#include "shill/dbus_service_proxy_interface.h"
#include "shill/error.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"

using base::Bind;
using base::Unretained;
using std::list;
using std::map;
using std::string;

namespace shill {

namespace {

const int kDefaultRPCTimeoutMS = 30000;

}  // namespace

DBusManager::DBusManager()
    : proxy_factory_(ProxyFactory::GetInstance()) {}

DBusManager::~DBusManager() {}

void DBusManager::Start() {
  SLOG(DBus, 2) << __func__;
  if (proxy_.get()) {
    return;
  }
  proxy_.reset(proxy_factory_->CreateDBusServiceProxy());
  proxy_->set_name_owner_changed_callback(
      Bind(&DBusManager::OnNameOwnerChanged, Unretained(this)));
}

void DBusManager::Stop() {
  SLOG(DBus, 2) << __func__;
  proxy_.reset();
  name_watchers_.clear();
}

void DBusManager::WatchName(const string &name,
                            const AppearedCallback &on_appear,
                            const VanishedCallback &on_vanish) {
  LOG(INFO) << "DBus watch: " << name;
  NameWatcher watcher(on_appear, on_vanish);
  name_watchers_[name].push_back(watcher);
  Error error;
  proxy_->GetNameOwner(
      name,
      &error,
      Bind(&DBusManager::OnGetNameOwnerComplete, Unretained(this), watcher),
      kDefaultRPCTimeoutMS);
  // Ensures that the watcher gets an initial appear/vanish notification
  // regardless of the outcome of the GetNameOwner call.
  if (error.IsFailure()) {
    OnGetNameOwnerComplete(watcher, string(), error);
  }
}

void DBusManager::OnNameOwnerChanged(
    const string &name, const string &old_owner, const string &new_owner) {
  map<string, list<NameWatcher> >::const_iterator find_it =
      name_watchers_.find(name);
  if (find_it == name_watchers_.end()) {
    return;
  }
  LOG(INFO) << "DBus name owner changed: " << name
            << "(" << old_owner << "->" << new_owner << ")";
  for (list<NameWatcher>::const_iterator it = find_it->second.begin();
       it != find_it->second.end(); ++it) {
    NotifyNameWatcher(*it, new_owner);
  }
}

void DBusManager::OnGetNameOwnerComplete(const NameWatcher &watcher,
                                         const string &unique_name,
                                         const Error &error) {
  LOG(INFO) << "DBus name owner: " << unique_name
            << "(" << error.message() << ")";
  NotifyNameWatcher(watcher, error.IsSuccess() ? unique_name : string());
}

// static
void DBusManager::NotifyNameWatcher(const NameWatcher &watcher,
                                    const string &owner) {
  SLOG(DBus, 2) << __func__ << "(" << owner << ")";
  if (owner.empty()) {
    if (!watcher.on_vanish.is_null()) {
      watcher.on_vanish.Run();
    }
  } else if (!watcher.on_appear.is_null()) {
    watcher.on_appear.Run(owner);
  }
}

}  // namespace shill
