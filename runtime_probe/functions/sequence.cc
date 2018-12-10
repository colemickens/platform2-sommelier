/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "runtime_probe/functions/sequence.h"

namespace runtime_probe {

SequenceFunction::DataType SequenceFunction::Eval() const {
  DataType return_value{};
  return_value.emplace_back();  // initially, there is an empty dict in list.

  for (const auto& func : functions_) {
    const auto& probe_results = func->Eval();

    if (probe_results.size() == 0)
      return {};

    if (probe_results.size() > 1) {
      LOG(ERROR) << "Subfunction call generates more than one results";
      return {};
    }

    return_value.back().MergeDictionary(&probe_results[0]);
  }

  return return_value;
}

}  // namespace runtime_probe
