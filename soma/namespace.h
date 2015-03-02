// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_NAMESPACE_H_
#define SOMA_NAMESPACE_H_

#include <sched.h>

#include <set>

namespace base {
class ListValue;
}

namespace soma {
namespace ns {
extern const char kListKey[];
extern const char kNewIpc[];
extern const char kNewNet[];
extern const char kNewNs[];
extern const char kNewPid[];
extern const char kNewUser[];
extern const char kNewUts[];

enum class Kind {
  NEWIPC = CLONE_NEWIPC,
  NEWNET = CLONE_NEWNET,
  NEWNS = CLONE_NEWNS,
  NEWPID = CLONE_NEWPID,
  NEWUSER = CLONE_NEWUSER,
  NEWUTS = CLONE_NEWUTS,
  INVALID
};

std::set<ns::Kind> ParseList(base::ListValue* namepaces);
}  // namespace ns
}  // namespace soma
#endif  // SOMA_NAMESPACE_H_
