// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OOBE_CONFIG_OOBE_CONFIG_CONSTANTS_H_
#define OOBE_CONFIG_OOBE_CONFIG_CONSTANTS_H_

#include <base/files/file_util.h>

namespace oobe_config {

const base::FilePath kInstallAttributesPath(
    "/home/.shadow/install_attributes.pb");
const base::FilePath kOwnerKeyPath("/var/lib/whitelist/owner.key");
const base::FilePath kPolicyPath("/var/lib/whitelist/policy");
const base::FilePath kShillDefaultProfilePath(
    "/var/cache/shill/default.profile");

const base::FilePath kRollbackDataPath(
    "/mnt/stateful_partition/unencrypted/preserve/rollback_data");

const base::FilePath kPolicyDotOnePathForTesting("/var/lib/whitelist/policy.1");

}  // namespace oobe_config

#endif  // OOBE_CONFIG_OOBE_CONFIG_CONSTANTS_H_
