// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/namespace.h"

#include <set>
#include <string>

#include <base/logging.h>
#include <base/values.h>

namespace soma {
namespace ns {
const char kListKey[] = "namespaces";
const char kNewIpc[] = "CLONE_NEWIPC";
const char kNewNet[] = "CLONE_NEWNET";
const char kNewNs[] = "CLONE_NEWNS";
const char kNewPid[] = "CLONE_NEWPID";
const char kNewUser[] = "CLONE_NEWUSER";
const char kNewUts[] = "CLONE_NEWUTS";

namespace {
ns::Kind Resolve(const std::string& namespace_string) {
  if (namespace_string == kNewIpc)
    return ns::Kind::NEWIPC;
  else if (namespace_string == kNewNet)
    return ns::Kind::NEWNET;
  else if (namespace_string == kNewNs)
    return ns::Kind::NEWNS;
  else if (namespace_string == kNewPid)
    return ns::Kind::NEWPID;
  else if (namespace_string == kNewUser)
    return ns::Kind::NEWUSER;
  else if (namespace_string == kNewUts)
    return ns::Kind::NEWUTS;

  return ns::Kind::INVALID;
}
}  // anonymous namespace

std::set<ns::Kind> ParseList(base::ListValue* namespaces) {
  std::set<ns::Kind> to_return;
  std::string namespace_string;
  for (base::Value* namespace_value : *namespaces) {
    if (!namespace_value->GetAsString(&namespace_string)) {
      LOG(ERROR) << "Namespace specifiers must be strings";
      continue;
    }
    ns::Kind ns = Resolve(namespace_string);
    if (ns != ns::Kind::INVALID)
      to_return.insert(ns);
  }
  return to_return;
}

}  // namespace ns
}  // namespace soma
