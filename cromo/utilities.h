// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities for plugins for the cromo modem manager.

#ifndef CROMO_UTILITIES_H_
#define CROMO_UTILITIES_H_

#include <base/basictypes.h>
#include <map>
#include <string>

#include <dbus/dbus.h>
#include <dbus-c++/dbus.h>

namespace utilities {

typedef std::map<std::string, DBus::Variant> DBusPropertyMap;

// Extracts the key from properties, returning not_found_response if
// the key is not found.  If key is found, but is not a string, sets
// error and returns not_found_response.  If error.is_set() is true,
// ExtractString will not report further errors.  You can make
// multiple ExtractString calls and check error at the end.
const char* ExtractString(const DBusPropertyMap properties,
                          const char* key,
                          const char* not_found_response,
                          DBus::Error& error);

// Extracts the key from properties, returning not_found_response if
// the key is not found.  If key is found, but is not a Uint32, sets
// error and returns not_found_response.  If error.is_set() is true,
// ExtractUint32 will not report further errors.  You can make
// multiple ExtractUint32 calls and check error at the end.
uint32_t ExtractUint32(const DBusPropertyMap properties,
                       const char* key,
                       uint32_t not_found_response,
                       DBus::Error& error);


// Convert a string representing a hex ESN to one representing a
// decimal ESN.  Returns success.
bool HexEsnToDecimal(const std::string& esn_hex, std::string* out);

// Converts an array of bytes containing text encoded in the GSM 03.38
// character set (also know as GSM-7) into a UTF-8 encoded string.
// GSM-7 is a 7-bit character set, and in SMS messages, the 7-bit
// septets are packed into an array of 8-bit octets.
//
// The first byte of the input array gives the length of the converted
// data in septets, i.e., it is the number of GSM-7 septets that
// will result after the array is unpacked.
std::string Gsm7ToUtf8String(const uint8_t* gsm7);

// Converts a string of characters encoded using UTF-8 into an
// array of bytes which is result of converting the string into
// septets in the GSM-7 alphabet and then packing the septets into
// octets.
std::vector<uint8_t> Utf8StringToGsm7(const std::string& input);

// Converts an array of bytes containing text in the UCS-2 encoding
// into a UTF-8 encoded string.
//
// The first byte of the input array gives the length of the converted
// data in octets. Dividing this number by 2 gives the number of
// characters in the text.
std::string Ucs2ToUtf8String(const uint8_t* ucs2);

// Convert a UTF-8 encoded string to a byte array encoding
// the string as UCS-2.
std::vector<uint8_t> Utf8StringToUcs2(const std::string& input);

// Debugging utility for printing an array of bytes in a nicely
// formatted manner á la the UNIX hd command.
void DumpHex(const uint8_t* buf, size_t size);

}  // namespace utilities

#endif  /* CROMO_UTILITIES_H_ */
