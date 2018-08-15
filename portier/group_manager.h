// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_GROUP_MANAGER_H_
#define PORTIER_GROUP_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <base/stl_util.h>

#include "portier/group.h"
#include "portier/status.h"

namespace portier {

// Manages proxy interfaces and proxy groups.
// Proxy groups are logical groupings of interfaces which collectively
// act as a Neighbor Discover proxy node.  Interfaces can only be part
// of one group.  Destroying a group will remove all member interfaces.
template <class MemberType>
class GroupManager {
 public:
  using GroupType = Group<MemberType>;

  GroupManager() = default;
  ~GroupManager() = default;

  // Proxy groups.
  // Creates a new proxy group.  Verifies that the name is valid and
  // that no group exists with the current name.  Store the group
  // internally, but can be accessed using a call to `GetGroup()`.
  Status CreateGroup(const std::string& pg_name);

  // Stops managing a proxy group, removing all members from the group.
  Status ReleaseGroup(const std::string& pg_name);

  // Releases all proxy groups, removing all members.
  void ReleaseAllGroups();

  // Checks if a given proxy group exists already.
  bool HasGroup(const std::string& pg_name) const;

  // Get a pointer to the group.
  GroupType* GetGroup(const std::string& pg_name) const;

  // Get a list of the existing proxy groups.
  std::vector<std::string> GetGroupNames() const;

  // Gets the list of proxy groups.
  std::vector<GroupType*> GetGroups() const;

 private:
  using NameGroupPair =
      std::pair<const std::string, std::unique_ptr<GroupType>>;
  using Code = Status::Code;

  // A mapping of group names to a list of their members.
  std::map<std::string, std::unique_ptr<GroupType>> proxy_groups_;

  DISALLOW_COPY_AND_ASSIGN(GroupManager);
};

// Template implementation.

template <class MemberType>
Status GroupManager<MemberType>::CreateGroup(const std::string& pg_name) {
  if (HasGroup(pg_name)) {
    return Status(Code::ALREADY_EXISTS)
           << "A proxy group named " << pg_name << " already exists";
  }

  auto pg_ptr = GroupType::Create(pg_name);

  // Only cause of failure is having an invalid name.
  if (!pg_ptr) {
    return Status(Code::INVALID_ARGUMENT)
           << "Invalid proxy group name " << pg_name;
  }

  proxy_groups_.insert(NameGroupPair(pg_name, std::move(pg_ptr)));
  return Status();
}

template <class MemberType>
Status GroupManager<MemberType>::ReleaseGroup(const std::string& pg_name) {
  auto it = proxy_groups_.find(pg_name);
  if (it == proxy_groups_.end()) {
    return Status(Code::DOES_NOT_EXIST)
           << "The proxy group " << pg_name << " does not exist";
  }

  it->second.get()->RemoveAllMembers();
  proxy_groups_.erase(it);
  return Status();
}

template <class MemberType>
void GroupManager<MemberType>::ReleaseAllGroups() {
  auto pg_names = GetGroupNames();
  for (const auto& pg_name : pg_names) {
    ReleaseGroup(pg_name);
  }
}

template <class MemberType>
bool GroupManager<MemberType>::HasGroup(const std::string& pg_name) const {
  return base::ContainsKey(proxy_groups_, pg_name);
}

template <class MemberType>
Group<MemberType>* GroupManager<MemberType>::GetGroup(
    const std::string& pg_name) const {
  const auto it = proxy_groups_.find(pg_name);
  if (it == proxy_groups_.end()) {
    return nullptr;
  }
  return it->second.get();
}

template <class MemberType>
std::vector<std::string> GroupManager<MemberType>::GetGroupNames() const {
  std::vector<std::string> pg_names;
  for (const NameGroupPair& pair : proxy_groups_) {
    pg_names.push_back(pair.first);
  }
  return pg_names;
}

template <class MemberType>
std::vector<Group<MemberType>*> GroupManager<MemberType>::GetGroups() const {
  std::vector<GroupType*> groups;
  for (const NameGroupPair& pair : proxy_groups_) {
    groups.push_back(pair.second.get());
  }
  return groups;
}

}  // namespace portier

#endif  // PORTIER_GROUP_MANAGER_H_
