// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BINDINGS_DBUS_SIGNATURE_H_
#define CHROMEOS_DBUS_BINDINGS_DBUS_SIGNATURE_H_

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace chromeos_dbus_bindings {

class DBusType {
 public:
  virtual ~DBusType() = default;

  // Some types might not be allowed in properties because libchrome bindings
  // don't support them, or they don't make any sense as properties. One
  // example would be file descriptors.
  virtual bool IsValidPropertyType() const = 0;

  // Methods for getting the C++ type corresponding to a D-Bus type.
  virtual std::string GetBaseType() const = 0;
  virtual std::string GetInArgType() const = 0;
  std::string GetOutArgType() const;
};

class DBusSignature {
 public:
  DBusSignature();
  virtual ~DBusSignature() = default;

  // Returns a DBusType corresponding to the D-Bus signature given in
  // |signature|. If the signature fails to parse, returns nullptr.
  std::unique_ptr<DBusType> Parse(const std::string& signature);
  bool Parse(const std::string& signature, std::string* output);

 private:
  friend class DBusSignatureTest;
  FRIEND_TEST(DBusSignatureTest, ParseSuccesses);

  // Typenames are C++ syntax types.
  static const char kArrayTypename[];
  static const char kBooleanTypename[];
  static const char kByteTypename[];
  static const char kObjectPathTypename[];
  static const char kDictTypename[];
  static const char kDoubleTypename[];
  static const char kSigned16Typename[];
  static const char kSigned32Typename[];
  static const char kSigned64Typename[];
  static const char kStringTypename[];
  static const char kUnixFdTypename[];
  static const char kUnsigned16Typename[];
  static const char kUnsigned32Typename[];
  static const char kUnsigned64Typename[];
  static const char kVariantTypename[];
  static const char kVariantDictTypename[];
  static const char kPairTypename[];
  static const char kTupleTypename[];

  // Returns an intermediate-representation type for the next D-Bus signature
  // in the string at |signature|, as well as the next position within the
  // string that parsing should continue |next|. Returns nullptr on failure.
  std::unique_ptr<DBusType> GetTypenameForSignature(
      std::string::const_iterator signature,
      std::string::const_iterator end,
      std::string::const_iterator* next);

  // Parses multiple types out of a D-Bus signature until it encounters an
  // |end_char| and places them in |children|. Returns true on success.
  bool ParseChildTypes(std::string::const_iterator signature,
                       std::string::const_iterator end,
                       std::string::value_type end_char,
                       std::string::const_iterator* next,
                       std::vector<std::unique_ptr<DBusType>>* children);

  // Utility task for GetTypenameForSignature() which handles array objects
  // and decodes them into a map or vector depending on the encoded sub-elements
  // in the array. The arguments and return values are the same
  // as GetTypenameForSignature().
  std::unique_ptr<DBusType> GetArrayTypenameForSignature(
      std::string::const_iterator signature,
      std::string::const_iterator end,
      std::string::const_iterator* next);

  // Utility task for GetArrayTypenameForSignature() which handles dict objects.
  std::unique_ptr<DBusType> GetDictTypenameForSignature(
      std::string::const_iterator signature,
      std::string::const_iterator end,
      std::string::const_iterator* next);

  // Utility task for GetTypenameForSignature() which handles structs.
  // The arguments and return values are the same as GetTypenameForSignature().
  std::unique_ptr<DBusType> GetStructTypenameForSignature(
      std::string::const_iterator signature,
      std::string::const_iterator end,
      std::string::const_iterator* next);

  DISALLOW_COPY_AND_ASSIGN(DBusSignature);
};

}  // namespace chromeos_dbus_bindings

#endif  // CHROMEOS_DBUS_BINDINGS_DBUS_SIGNATURE_H_
