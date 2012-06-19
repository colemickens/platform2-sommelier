// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_MANAGER_H_
#define SHILL_DBUS_MANAGER_H_

#include <list>
#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/callback.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace shill {

class DBusServiceProxyInterface;
class Error;
class ProxyFactory;

class DBusManager {
 public:
  typedef base::Callback<void(const std::string &owner)> AppearedCallback;
  typedef base::Closure VanishedCallback;

  DBusManager();
  ~DBusManager();

  void Start();
  void Stop();

  // Registers a watcher for DBus service |name|. When the service appears,
  // |on_appear| is invoked if non-null. When the service vanishes, |on_vanish|
  // is invoked if non-null. |on_appear| or |on_vanish| will be notified once
  // asynchronously if the service has or doesn't have an owner, respectively,
  // when WatchName is invoked.
  void WatchName(const std::string &name,
                 const AppearedCallback &on_appear,
                 const VanishedCallback &on_vanish);

 private:
  friend class DBusManagerTest;
  FRIEND_TEST(DBusManagerTest, NameWatchers);

  struct NameWatcher {
    NameWatcher(const AppearedCallback &on_appear_in,
                const VanishedCallback &on_vanish_in)
        : on_appear(on_appear_in), on_vanish(on_vanish_in) {}

    AppearedCallback on_appear;
    VanishedCallback on_vanish;
  };

  void OnNameOwnerChanged(const std::string &name,
                          const std::string &old_owner,
                          const std::string &new_owner);

  void OnGetNameOwnerComplete(const NameWatcher &watcher,
                              const std::string &unique_name,
                              const Error &error);

  static void NotifyNameWatcher(const NameWatcher &watcher,
                                const std::string &owner);

  ProxyFactory *proxy_factory_;

  scoped_ptr<DBusServiceProxyInterface> proxy_;

  std::map<std::string, std::list<NameWatcher> > name_watchers_;

  DISALLOW_COPY_AND_ASSIGN(DBusManager);
};

}  // namespace shill

#endif  // SHILL_DBUS_MANAGER_H_
