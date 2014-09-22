// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_VARIANT_DICTIONARY_H_
#define LIBCHROMEOS_CHROMEOS_VARIANT_DICTIONARY_H_

#include <map>
#include <string>

#include <chromeos/any.h>
#include <chromeos/chromeos_export.h>

namespace chromeos {

using VariantDictionary = std::map<std::string, chromeos::Any>;

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_VARIANT_DICTIONARY_H_
