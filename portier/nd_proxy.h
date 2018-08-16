// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_ND_PROXY_H_
#define PORTIER_ND_PROXY_H_

#include <map>
#include <memory>
#include <string>

#include <base/callback.h>
#include <base/macros.h>
#include <brillo/message_loops/message_loop.h>

#include "portier/neighbor_cache.h"
#include "portier/proxy_interface.h"
#include "portier/status.h"

namespace portier {

class NeighborDiscoveryProxy {
 public:
  static std::unique_ptr<NeighborDiscoveryProxy> Create(
      bool nested_mode = false);

  // The destructor will release all interfaces, release all groups,
  // and cancel all tasks associated to it.
  ~NeighborDiscoveryProxy();

  // Proxy interfaces.
  Status ManagerInterface(const std::string& if_name);
  Status ReleaseInterface(const std::string& if_name);
  bool IsManagingInterface(const std::string& if_name) const;

  // Proxy groups.
  Status CreateProxyGroup(const std::string& pg_name);
  Status ReleaseProxyGroup(const std::string& pg_name);
  bool HasProxyGroup(const std::string& pg_name) const;

  // Membership.
  Status AddToGroup(const std::string& if_name,
                    const std::string& pg_name,
                    bool as_upstream = false);
  Status RemoveFromGroup(const std::string& if_name);
  Status SetAsUpstream(const std::string& if_name);
  Status UnsetUpstream(const std::string& pg_name);

  bool IsNested() const { return nested_mode_; }
  void SetNestedMode(bool nested_mode) { nested_mode_ = nested_mode; }

  NeighborCache* GetNeighborCache() { return &neighbor_cache_; }
  ProxyGroupManager* GetProxyGroupManager() { return &group_manager_; }

 private:
  explicit NeighborDiscoveryProxy(bool nested_mode)
      : nested_mode_(nested_mode) {}

  // Returns a pointer to the interface of the given |if_name|.  If the
  // interface does not exists, then a nullptr is returned.
  std::shared_ptr<ProxyInterface> GetInterface(
      const std::string& if_name) const;

  // Marks the given |proxy_if| as having a loop and schedules a task
  // to call `LoopTimeOut() which clears the interface loop mark
  // after a set amount of time.  The TaskId of loop clearing task is
  // stored in |loop_tasks_| and must be removed should the interface
  // be removed or cleard of its loop mark elsewhere.
  //
  // See `LoopTimeOut()' for details.
  void HandleLoopDetection(std::shared_ptr<ProxyInterface> proxy_if);

  // Callbacks.

  // Main proxy logic.
  void HandleNeighborDiscoverPacket(std::string if_name);
  void HandleIPv6Packet(std::string if_name);

  // Clears the loop mark on an interface specified by the given
  // |if_name|.  This method is intended to be called only from a
  // scheduled task callback.
  // No action is take if:
  //  1 - The interface no longer exists,
  //  2 - The interface does not have a group,
  //  3 - The interface's group name does not match |pg_name|, or
  //  4 - There is no task Id found for the given |if_name|.
  // Note:
  // Because of case 2 and 3, it is the responsibility of this class
  // to cancel task should the interface be removed from a group and
  // readded or reactivated by some manual means.  Failure to do so
  // might cause a loop flag to be cleared prematurely.
  void LoopTimeOut(std::string if_name, std::string pg_name);

  // A special flag to indicate that the daemon is nested and can expect
  // to receive proxied RA requests.
  bool nested_mode_;

  NeighborCache neighbor_cache_;

  ProxyGroupManager group_manager_;

  // Maps file descripters to their associated watching task.  Used
  // for removing the FD from the watch pool when closed.
  std::map<int32_t, brillo::MessageLoop::TaskId> fd_tasks_;

  // Maps interface names to their task which handles the loop-timeout.
  // If a loop is detected on a downstream interface, then the
  // interface is marked as potentially having a loop.  A callback
  // is then scheduled to clear that flag after 1 hour.  Tracking
  // the task ID allows that task to be cleared should the interface
  // be released.
  std::map<std::string, brillo::MessageLoop::TaskId> loop_tasks_;

  // List of all managed proxy interfaces.
  std::map<std::string, std::shared_ptr<ProxyInterface>> proxy_ifs_;

  DISALLOW_COPY_AND_ASSIGN(NeighborDiscoveryProxy);
};

}  // namespace portier

#endif  // PORTIER_ND_PROXY_H_
