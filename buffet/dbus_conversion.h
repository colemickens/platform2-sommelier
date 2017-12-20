// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_DBUS_CONVERSION_H_
#define BUFFET_DBUS_CONVERSION_H_

#include <memory>

#include <base/values.h>
#include <brillo/any.h>
#include <brillo/errors/error.h>
#include <brillo/variant_dictionary.h>

namespace buffet {

// Converts DictionaryValue to D-Bus variant dictionary.
brillo::VariantDictionary DictionaryToDBusVariantDictionary(
    const base::DictionaryValue& object);

// Converts D-Bus variant dictionary to DictionaryValue.
std::unique_ptr<base::DictionaryValue> DictionaryFromDBusVariantDictionary(
    const brillo::VariantDictionary& object, brillo::ErrorPtr* error);

}  // namespace buffet

#endif  // BUFFET_DBUS_CONVERSION_H_
