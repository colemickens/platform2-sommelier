// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/namespace.h"

#include <set>
#include <string>

#include <base/logging.h>
#include <base/values.h>

namespace soma {
namespace parser {
namespace ns {
const char kListKey[] = "namespaces";
const char kNewIpc[] = "CLONE_NEWIPC";
const char kNewNet[] = "CLONE_NEWNET";
const char kNewNs[] = "CLONE_NEWNS";
const char kNewPid[] = "CLONE_NEWPID";
const char kNewUser[] = "CLONE_NEWUSER";
const char kNewUts[] = "CLONE_NEWUTS";

namespace {
bool Resolve(const std::string& namespace_string, Kind* out) {
  DCHECK(out);
  if (namespace_string == kNewIpc)
    *out = ContainerSpec::NEWIPC;
  else if (namespace_string == kNewNet)
    *out = ContainerSpec::NEWNET;
  else if (namespace_string == kNewNs)
    *out = ContainerSpec::NEWNS;
  else if (namespace_string == kNewPid)
    *out = ContainerSpec::NEWPID;
  else if (namespace_string == kNewUser)
    *out = ContainerSpec::NEWUSER;
  else if (namespace_string == kNewUts)
    *out = ContainerSpec::NEWUTS;
  else
    return false;
  return true;
}
}  // anonymous namespace

std::set<Kind> ParseList(const base::ListValue* namespaces) {
  std::set<Kind> to_return;
  std::string namespace_string;
  for (base::Value* namespace_value : *namespaces) {
    if (!namespace_value->GetAsString(&namespace_string)) {
      LOG(ERROR) << "Namespace specifiers must be strings";
      continue;
    }
    Kind ns;
    if (Resolve(namespace_string, &ns))
      to_return.insert(ns);
  }
  return to_return;
}

}  // namespace ns
}  // namespace parser
}  // namespace soma
