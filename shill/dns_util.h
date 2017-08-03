//
// Copyright 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_DNS_UTIL_H_
#define SHILL_DNS_UTIL_H_

#include <string>

#include <base/strings/string_piece.h>

namespace shill {
// TODO(crbug.com/751899): The DNS name validation code is adapted from
// dns_util* in Chrome:
//
// https://chromium.googlesource.com/chromium/src/+/3674d6f0ac52b4c7e3c21aa76f1cf842692ec692/net/dns/dns_util.h
//
// It would be better to include it in libchrome so that the code is
// maintained in one place.

// DNSDomainFromDot - convert a domain string to DNS format. From DJB's
// public domain DNS library.
//
//   dotted: a string in dotted form: "www.google.com"
//   out: a result in DNS form: "\x03www\x06google\x03com\x00"
bool DNSDomainFromDot(const base::StringPiece& dotted, std::string* out);

// Checks that a hostname is valid. Simple wrapper around DNSDomainFromDot.
bool IsValidDNSDomain(const base::StringPiece& dotted);

// Returns true if the character is valid in a DNS hostname label, whether in
// the first position or later in the label.
//
// This function asserts a looser form of the restrictions in RFC 7719 (section
// 2; https://tools.ietf.org/html/rfc7719#section-2): hostnames can include
// characters a-z, A-Z, 0-9, -, and _, and any of those characters (except -)
// are legal in the first position. The looser rules are necessary to support
// service records (initial _), and non-compliant but attested hostnames that
// include _. These looser rules also allow Punycode and hence IDN.
bool IsValidHostLabelCharacter(char c, bool is_first_char);

}  // namespace shill

#endif  // SHILL_DNS_UTIL_H_
