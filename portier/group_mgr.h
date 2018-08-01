// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_GROUP_MGR_H_
#define PORTIER_GROUP_MGR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "portier/status.h"

namespace portier {

// Manages proxy interfaces and proxy groups.
// Proxy groups are logical groupings of interfaces which collectively
// act as a Neighbor Discover proxy node.  Interfaces can only be part
// of one group.  Destroying a group will remove all member interfaces.
class GroupManager {
 public:
  GroupManager();

  // Proxy groups.
  // Creates a new proxy group.  Verifies that the name is valid and
  // that no group exists with the current name.
  Status CreateProxyGroup(const std::string& pg_name);

  // Destroys an existing proxy group, removing all members from the group.
  Status DestroyProxyGroup(const std::string& pg_name);

  // Destroys all proxy groups, removing all members.
  void DestroyAllProxyGroups();

  // Checks if a given proxy group exists already.
  bool HasProxyGroup(const std::string& pg_name) const;

  // Get a list of the existing proxy groups.
  std::vector<std::string> GetGroupNames() const;

  // Group membership.

  // Adds an interface to a proxy group.  Returns true on success, false if
  // request fails.  Can fail if the group does not exists or the interface
  // is already part of a different proxy group.
  Status AddInterfaceToProxyGroup(const std::string& if_name,
                                  const std::string& pg_name);

  // Removes an interface, by interface name, from the specified proxy group.
  // Call will fail if the proxy group does not exists or the interface is not
  // part of group.
  Status RemoveInterfaceFromProxyGroup(const std::string& if_name,
                                       const std::string& pg_name);

  // Checks if an interface is managed by some group.
  bool IsInterfaceMember(const std::string& if_name) const;

  // Checks if a given interface is a member of the given proxy group.
  bool IsInterfaceMemberOfProxyGroup(const std::string& if_name,
                                     const std::string& pg_name) const;

  // Returns a list of all of the interfaces specified in a group.
  // If the group does not exist, then an empty list is returned.
  Status GetGroupMembers(const std::string& pg_name,
                         std::vector<std::string>* if_names_out) const;

  // Get the name of the group of the given proxy interface.  The
  // specified interface must exists.
  bool GetProxyGroupOfInterface(const std::string& if_name,
                                std::string* pg_name) const;

  // Group upstream membership.

  // Check if a specific interface is the upstream interface of a
  // specific proxy group.  Returns false if either the interface or
  // the proxy group to not exist.
  bool IsInterfaceUpstream(const std::string& if_name,
                           const std::string& pg_name) const;

  // Get the name of the upstream interface for a specific proxy group.
  // Return false if the group does not have an upstream interface, or
  // if the specified group does not exist.
  bool GetProxyGroupUpstream(const std::string& pg_name,
                             std::string* if_name_out) const;

  // Set the upstream interface for the specified group.  |if_name|
  // must specify a member interface of the proxy group.  Call will
  // fail if the group or interface does not exist, if the group
  // already has an upstream interface set or if the specified
  // interface is not a member of the specified group.
  Status SetProxyGroupUpstream(const std::string& if_name,
                               const std::string& pg_name);

  // Remove an upstream interface for the group.
  Status RemoveProxyGroupUpstream(const std::string& pg_name);

 private:
  // A mapping of group names to a list of their members.
  std::map<std::string, std::vector<std::string>> proxy_groups_;

  // A mapping of proxy groups to the group's upstream interface.
  std::map<std::string, std::string> proxy_group_upstreams_;

  // A mapping of interface names to their group name.
  std::map<std::string, std::string> proxy_memberships_;
};

}  // namespace portier

#endif  // PORTIER_GROUP_MGR_H_
