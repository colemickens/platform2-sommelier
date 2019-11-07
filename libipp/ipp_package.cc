// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libipp/ipp_package.h"

namespace ipp {

// Methods for Package

Group* Package::GetGroup(GroupTag gn) {
  const std::vector<Group*> groups = GetKnownGroups();
  for (auto g : groups)
    if (g->GetName() == gn)
      return g;
  for (auto g : unknown_groups_)
    if (g->GetName() == gn)
      return g;
  return nullptr;
}

Group* Package::AddUnknownGroup(GroupTag gn, bool is_a_set) {
  if (GetGroup(gn) != nullptr)
    return nullptr;
  unknown_groups_.push_back(new UnknownGroup(gn, is_a_set));
  return unknown_groups_.back();
}

std::vector<Group*> Package::GetAllGroups() {
  std::vector<Group*> groups = GetKnownGroups();
  groups.insert(groups.end(), unknown_groups_.begin(), unknown_groups_.end());
  return groups;
}

Package::~Package() {
  for (auto g : unknown_groups_)
    delete g;
}

}  // namespace ipp
