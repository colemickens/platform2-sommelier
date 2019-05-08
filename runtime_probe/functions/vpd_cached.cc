// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "runtime_probe/functions/vpd_cached.h"
#include "runtime_probe/utils/file_utils.h"

namespace runtime_probe {

using base::DictionaryValue;
using base::Value;

// A dummy implementation that does not actually read vpd data
VPDCached::DataType VPDCached::Eval() const {
  DataType result{};

  DictionaryValue dict_with_prefix;
  dict_with_prefix.SetString("vpd_" + vpd_name_, "");

  result.push_back(std::move(dict_with_prefix));

  return result;
}

}  // namespace runtime_probe
