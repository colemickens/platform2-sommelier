// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/group_mgr.h"

#include <algorithm>
#include <utility>

#include <base/logging.h>
#include <base/stl_util.h>
#include <base/strings/string_util.h>

namespace portier {

using std::string;
using std::vector;
using std::shared_ptr;

using Code = Status::Code;

using NameList = vector<string>;
using GroupListPair = std::pair<const string, NameList>;
using NameNamePair = std::pair<const string, string>;

namespace {

// Groups names should be easy to type/remember group names.  These
// names will likely be typed on a shell.  Group names can contain
// alphanumeric characters or underscores.
bool IsValidGroupName(const string& pg_name) {
  for (char c : pg_name) {
    if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) && c != '_' &&
        c != '-') {
      return false;
    }
  }
  return pg_name.size() > 0;
}

}  // namespace

// Constructor.
GroupManager::GroupManager() {}

// Proxy groups.

Status GroupManager::CreateProxyGroup(const string& pg_name) {
  if (HasProxyGroup(pg_name)) {
    return Status(Code::ALREADY_EXISTS)
           << "A proxy group named " << pg_name << " already exists";
  }
  if (!IsValidGroupName(pg_name)) {
    return Status(Code::INVALID_ARGUMENT)
           << "Invalid proxy group name " << pg_name;
  }

  proxy_groups_.insert(GroupListPair(pg_name, NameList()));
  return Status();
}

Status GroupManager::DestroyProxyGroup(const string& pg_name) {
  if (!HasProxyGroup(pg_name)) {
    return Status(Code::DOES_NOT_EXIST)
           << "The proxy group " << pg_name << " does not exist";
  }

  // Step 1: Remove all interfaces from the proxy group.
  // Use copy of list as the contents will change during the loop.
  auto if_names = proxy_groups_.at(pg_name);
  for (const string& if_name : if_names) {
    RemoveInterfaceFromProxyGroup(if_name, pg_name);
  }

  // Step 2: Remove the proxy group from the list of groups.
  proxy_groups_.erase(pg_name);

  return Status();
}

void GroupManager::DestroyAllProxyGroups() {
  auto pg_names = GetGroupNames();
  for (const auto& pg_name : pg_names) {
    DestroyProxyGroup(pg_name);
  }
}

bool GroupManager::HasProxyGroup(const string& pg_name) const {
  return base::ContainsKey(proxy_groups_, pg_name);
}

vector<string> GroupManager::GetGroupNames() const {
  vector<string> pg_names;
  for (const GroupListPair& pair : proxy_groups_) {
    pg_names.push_back(pair.first);
  }
  return pg_names;
}

// Group membership.

Status GroupManager::AddInterfaceToProxyGroup(const string& if_name,
                                              const string& pg_name) {
  if (!HasProxyGroup(pg_name)) {
    return Status(Code::DOES_NOT_EXIST)
           << "The proxy group " << pg_name << " does not exist";
  }
  if (IsInterfaceMember(if_name)) {
    string other_pg_name;
    GetProxyGroupOfInterface(if_name, &other_pg_name);
    return Status(Code::ALREADY_EXISTS)
           << "Interface " << if_name << " is already a member of group "
           << other_pg_name;
  }

  proxy_groups_.at(pg_name).push_back(if_name);
  proxy_memberships_.insert(NameNamePair(if_name, pg_name));
  return Status();
}

Status GroupManager::RemoveInterfaceFromProxyGroup(const string& if_name,
                                                   const string& pg_name) {
  if (!IsInterfaceMember(if_name)) {
    return Status(Code::DOES_NOT_EXIST)
           << "Interface " << if_name << " is not a member of any group";
  }
  if (!HasProxyGroup(pg_name)) {
    return Status(Code::DOES_NOT_EXIST)
           << "Proxy group " << pg_name << " does not exist";
  }
  if (!IsInterfaceMemberOfProxyGroup(if_name, pg_name)) {
    return Status(Code::DOES_NOT_EXIST)
           << "Interface " << if_name << " is not a member of the proxy group "
           << pg_name;
  }

  // If interface is upstream, then remove it as upstream.
  if (IsInterfaceMemberOfProxyGroup(if_name, pg_name)) {
    proxy_group_upstreams_.erase(pg_name);
  }

  auto& members = proxy_groups_.at(pg_name);
  for (auto it = members.begin(); it != members.end(); it++) {
    if (*it == if_name) {
      members.erase(it);
      break;
    }
  }
  proxy_memberships_.erase(if_name);
  return Status();
}

bool GroupManager::IsInterfaceMember(const string& if_name) const {
  return base::ContainsKey(proxy_memberships_, if_name);
}

bool GroupManager::IsInterfaceMemberOfProxyGroup(const string& if_name,
                                                 const string& pg_name) const {
  if (!HasProxyGroup(pg_name)) {
    return false;
  }
  if (!IsInterfaceMember(if_name)) {
    return false;
  }
  return proxy_memberships_.at(if_name) == pg_name;
}

Status GroupManager::GetGroupMembers(const string& pg_name,
                                     vector<string>* if_names_out) const {
  DCHECK(if_names_out != nullptr);
  if (!HasProxyGroup(pg_name)) {
    return Status(Code::DOES_NOT_EXIST)
           << "Proxy group " << pg_name << " does not exist";
  }
  *if_names_out = proxy_groups_.at(pg_name);
  return Status();
}

bool GroupManager::GetProxyGroupOfInterface(const string& if_name,
                                            string* pg_name) const {
  DCHECK(pg_name != nullptr);
  if (!IsInterfaceMember(if_name)) {
    return false;
  }
  *pg_name = proxy_memberships_.at(if_name);
  return true;
}

// Group upstream interface membership.

bool GroupManager::IsInterfaceUpstream(const std::string& if_name,
                                       const std::string& pg_name) const {
  if (!base::ContainsKey(proxy_group_upstreams_, pg_name)) {
    return false;
  }
  return proxy_group_upstreams_.at(pg_name) == if_name;
}

bool GroupManager::GetProxyGroupUpstream(const string& pg_name,
                                         string* if_name) const {
  DCHECK(if_name != nullptr);
  if (!HasProxyGroup(pg_name) ||
      !base::ContainsKey(proxy_group_upstreams_, pg_name)) {
    return false;
  }
  *if_name = proxy_group_upstreams_.at(pg_name);
  return true;
}

Status GroupManager::SetProxyGroupUpstream(const string& if_name,
                                           const string& pg_name) {
  if (!IsInterfaceMember(if_name)) {
    return Status(Code::DOES_NOT_EXIST)
           << "Interface " << if_name << " is not a member of any group";
  }
  if (!HasProxyGroup(pg_name)) {
    return Status(Code::DOES_NOT_EXIST)
           << "Proxy group " << pg_name << " does not exist";
  }
  if (!IsInterfaceMemberOfProxyGroup(if_name, pg_name)) {
    return Status(Code::DOES_NOT_EXIST)
           << "Interface " << if_name << " is not a member of proxy group "
           << pg_name;
  }
  if (base::ContainsKey(proxy_group_upstreams_, pg_name)) {
    string upstream;
    GetProxyGroupUpstream(pg_name, &upstream);
    return Status(Code::ALREADY_EXISTS)
           << "Proxy group " << pg_name << " already has an upstream interface "
           << upstream;
  }

  proxy_group_upstreams_.insert(NameNamePair(pg_name, if_name));
  return Status();
}

Status GroupManager::RemoveProxyGroupUpstream(const std::string& pg_name) {
  if (!HasProxyGroup(pg_name)) {
    return Status(Code::DOES_NOT_EXIST)
           << "Proxy group " << pg_name << " does not exist";
  }

  if (base::ContainsKey(proxy_group_upstreams_, pg_name)) {
    proxy_group_upstreams_.erase(pg_name);
  }

  return Status();
}

}  // namespace portier
