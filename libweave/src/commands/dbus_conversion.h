// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_COMMANDS_DBUS_CONVERSION_H_
#define LIBWEAVE_SRC_COMMANDS_DBUS_CONVERSION_H_

#include <base/values.h>
#include <chromeos/any.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>

#include "libweave/src/commands/schema_utils.h"

namespace weave {

// Converts DictionaryValue to D-Bus variant dictionary.
chromeos::VariantDictionary DictionaryToDBusVariantDictionary(
    const base::DictionaryValue& object);

// Converts D-Bus variant dictionary to DictionaryValue.
std::unique_ptr<base::DictionaryValue> DictionaryFromDBusVariantDictionary(
    const chromeos::VariantDictionary& object,
    chromeos::ErrorPtr* error);

}  // namespace weave

#endif  // LIBWEAVE_SRC_COMMANDS_DBUS_CONVERSION_H_
