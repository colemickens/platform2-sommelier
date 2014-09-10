// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BINDINGS_METHOD_NAME_GENERATOR_H_
#define CHROMEOS_DBUS_BINDINGS_METHOD_NAME_GENERATOR_H_

#include <string>

#include <base/macros.h>

namespace base {

class FilePath;

}  // namespace base

namespace chromeos_dbus_bindings {

struct Interface;

class MethodNameGenerator {
 public:
  MethodNameGenerator() = default;
  virtual ~MethodNameGenerator() = default;

  virtual bool GenerateMethodNames(const Interface &interface,
                                   const base::FilePath& output_file);
  static std::string GenerateMethodNameConstant(const std::string& method_name);

 private:
  friend class MethodNameGeneratorTest;

  DISALLOW_COPY_AND_ASSIGN(MethodNameGenerator);
};

}  // namespace chromeos_dbus_bindings

#endif  // CHROMEOS_DBUS_BINDINGS_METHOD_NAME_GENERATOR_H_
