// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RUNTIME_PROBE_STATEMENT_PARSER_H_
#define RUNTIME_PROBE_STATEMENT_PARSER_H_

#include <memory>
#include <string>
#include <base/values.h>

namespace runtime_probe {

// Parse |config_file_path|, the path of file containing probe statement in JSON
std::unique_ptr<base::DictionaryValue> ParseProbeConfig(
    const std::string& config_file_path);

}  // namespace runtime_probe

#endif  // RUNTIME_PROBE_STATEMENT_PARSER_H_
