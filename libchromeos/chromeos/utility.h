// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_UTILITY_H_
#define LIBCHROMEOS_CHROMEOS_UTILITY_H_

#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <base/values.h>

// Be VERY CAREFUL when touching this. It is include-order-sensitive.
// This library (libchromeos) is built with -fno-exceptions, but dbus-c++ uses
// exceptions heavily. In dbus-c++/types.h, there are overloaded operators for
// accessing DBus::MessageIters, which causes compilation to fail when they're
// included into libchromeos. Therefore, define away the throws in the header
// file, so that:
//    throw Foo();
// becomes:
//    abort(), Foo();
// The comma keeps the resulting expression one statement (which makes unbraced
// if statements work properly).
//
// The extra twist is that dbus-c++/error.h has some incompatible uses of throw
// (like the throw annotation on method declarations), so we explicitly include
// dbus-c++/error.h first so its include guards will have been set, then include
// dbus-c++/types.h.
#include <dbus-c++/error.h>
#define throw abort(),
#include <dbus-c++/types.h>
#undef throw

// For use in a switch statement to return the string from a label. Like:
// const char* CommandToName(CommandType command) {
//    switch (command) {
//       CHROMEOS_CASE_RETURN_LABEL(CMD_DELETE);
//       CHROMEOS_CASE_RETURN_LABEL(CMD_OPEN);
//    }
//    return "Unknown commmand";
// }
#define CHROMEOS_CASE_RETURN_LABEL(label) \
    case label: return #label

namespace chromeos {

typedef std::vector<unsigned char> Blob;

// Returns a string that represents the hexadecimal encoded contents of blob.
// String will contain only the characters 0-9 and a-f.
//
// Parameters
//  blob   - The bytes to encode.
//  string - ASCII representation of the blob in hex.
//
std::string AsciiEncode(const Blob &blob);

// Converts a string representing a sequence of bytes in hex into the actual
// bytes.
//
// Parameters
//  str - The bytes to decode.
// Returns
//  The decoded bytes as a Blob.
//
Blob AsciiDecode(const std::string &str);

// Secure memset - volatile qualifier prevents a call to memset from being
// optimized away.
//
// Based on memset_s in:
// https://buildsecurityin.us-cert.gov/daisy/bsi-rules/home/g1/771-BSI.html
void* SecureMemset(void *v, int c, size_t n);

// Compare [n] bytes starting at [s1] with [s2] and return 0 if they match,
// 1 if they don't. Time taken to perform the comparison is only dependent on
// [n] and not on the relationship of the match between [s1] and [s2].
int SafeMemcmp(const void* s1, const void* s2, size_t n);

// Convert a DBus message into a Value.
//
// Parameters
//  message - Message to convert.
//  value - Result pointer.
// Returns
//  True if conversion succeeded, false if it didn't.
bool DBusMessageToValue(DBus::Message& message, base::Value** v);

// Convert a DBus message iterator a Value.
//
// Parameters
//  message - Message to convert.
//  value - Result pointer.
// Returns
//  True if conversion succeeded, false if it didn't.
bool DBusMessageIterToValue(DBus::MessageIter& message, base::Value** v);

// Convert a DBus property map to a Value.
//
// Parameters
//  message - Message to convert.
//  value - Result pointer.
// Returns
//  True if conversion succeeded, false if it didn't.
bool DBusPropertyMapToValue(std::map<std::string, DBus::Variant>&
                            properties, base::Value** v);

// Return a random printable string representing |len| bytes of randomness.
bool SecureRandomString(size_t len, std::string* result);

}  // namespace chromeos


#endif  // LIBCHROMEOS_CHROMEOS_UTILITY_H_
