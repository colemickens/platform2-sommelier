// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CALLBACKS_H_
#define SHILL_CALLBACKS_H_

#include <string>
#include <vector>

#include <base/callback.h>

#include "shill/accessor_interface.h"
#include "shill/key_value_store.h"

namespace shill {

class Error;
// Convenient typedefs for some commonly used callbacks.
typedef base::Callback<void(const Error&)> ResultCallback;
typedef base::Callback<void(const Error&, bool)> ResultBoolCallback;
typedef base::Callback<void(const Error&,
                            const std::string&)> ResultStringCallback;
typedef base::Callback<void(const Error&)> EnabledStateChangedCallback;
typedef base::Callback<void(const KeyValueStore&,
                            const Error&)> KeyValueStoreCallback;
typedef base::Callback<void(const std::vector<KeyValueStore>&,
                            const Error&)> KeyValueStoresCallback;
typedef base::Callback<void(const std::string&,
                            const Error&)> RpcIdentifierCallback;
typedef base::Callback<void(const std::string&, const Error&)> StringCallback;
typedef base::Callback<void(uint32_t, uint32_t, const KeyValueStore&)>
    ActivationStateSignalCallback;
typedef base::Callback<void(const Stringmaps&,
                            const Error&)> ResultStringmapsCallback;

}  // namespace shill

#endif  // SHILL_CALLBACKS_H_
