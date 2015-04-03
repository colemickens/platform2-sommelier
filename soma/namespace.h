// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_NAMESPACE_H_
#define SOMA_NAMESPACE_H_

#include <sched.h>

#include <set>

#include "soma/proto_bindings/soma_container_spec.pb.h"

namespace base {
class ListValue;
}

namespace soma {
namespace parser {
namespace ns {
extern const char kListKey[];
extern const char kNewIpc[];
extern const char kNewNet[];
extern const char kNewNs[];
extern const char kNewPid[];
extern const char kNewUser[];
extern const char kNewUts[];

using Kind = ContainerSpec::Namespace;

// Returns true if |namespaces| can be successfully parsed into |out|.
// False is returned on failure and |out| may be in an inconsistent state.
bool ParseList(const base::ListValue* namespaces, std::set<Kind>* out);

}  // namespace ns
}  // namespace parser
}  // namespace soma
#endif  // SOMA_NAMESPACE_H_
