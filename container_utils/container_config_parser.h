// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTAINER_UTILS_CONTAINER_CONFIG_PARSER_H_
#define CONTAINER_UTILS_CONTAINER_CONFIG_PARSER_H_

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/values.h>

#include "container_utils/oci_config.h"

namespace container_utils {

using OciConfigPtr = std::unique_ptr<OciConfig>;

// Parses container configuration from the config.json data as specified in
// https://github.com/opencontainers/runtime-spec/tree/v1.0.0-rc1
//  |config_json_data| - The text from config.json.
//  |config_out| - Filled with the OCI configuration.
bool ParseContainerConfig(const std::string& config_json_data,
                          OciConfigPtr const& config_out);

}  // namespace container_utils

#endif  // CONTAINER_UTILS_CONTAINER_CONFIG_PARSER_H_
