// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RUNTIME_PROBE_UTILS_VALUE_UTILS_H_
#define RUNTIME_PROBE_UTILS_VALUE_UTILS_H_

#include <string>

#include <base/values.h>

namespace runtime_probe {
// Append the given |prefix| to each key in the |dict_value|.
void PrependToDVKey(base::DictionaryValue* dict_value,
                    const std::string& prefix);

}  // namespace runtime_probe

#endif  // RUNTIME_PROBE_UTILS_VALUE_UTILS_H_
