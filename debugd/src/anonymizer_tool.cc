// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/anonymizer_tool.h"

#include <pcrecpp.h>

#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

using base::IntToString;
using base::StringPrintf;
using std::map;
using std::string;

namespace debugd {

namespace {

// The |kCustomPatterns| array defines patterns to match and anonymize. Each
// pattern needs to define three capturing parentheses groups:
//
// - a group for the pattern before the identifier to be anonymized;
// - a group for the identifier to be anonymized;
// - a group for the pattern after the identifier to be anonymized.
//
// Every matched identifier (in the context of the whole pattern) is anonymized
// by replacing it with an incremental instance identifier. Every different
// pattern defines a separate instance identifier space. See the unit test for
// AnonymizerTool::AnonymizeCustomPattern for pattern anonymization examples.
//
// Useful regular expression syntax:
//
// +? is a non-greedy (lazy) +.
// \b matches a word boundary.
// (?i) turns on case insensitivy for the remainder of the regex.
// (?-s) turns off "dot matches newline" for the remainder of the regex.
// (?:regex) denotes non-capturing parentheses group.
const char *kCustomPatterns[] = {
  "(\\bCell ID: ')([0-9a-fA-F]+)(')",  // ModemManager
  "(\\bLocation area code: ')([0-9a-fA-F]+)(')",  // ModemManager
  "(?i-s)(\\bssid[= ]')(.+)(')",  // wpa_supplicant
  "(?-s)(\\bSSID - hexdump\\(len=[0-9]+\\): )(.+)()",  // wpa_supplicant
  "(?-s)(\\[SSID=)(.+?)(\\])",  // shill
};

}  // namespace

AnonymizerTool::AnonymizerTool()
    : custom_patterns_(arraysize(kCustomPatterns)) {}

AnonymizerTool::~AnonymizerTool() {}

string AnonymizerTool::Anonymize(const string& input) {
  string anonymized = AnonymizeMACAddresses(input);
  anonymized = AnonymizeCustomPatterns(anonymized);
  return anonymized;
}

string AnonymizerTool::AnonymizeMACAddresses(const string& input) {
  // This regular expression finds the next MAC address. It splits the data into
  // a section preceding the MAC address, an OUI (Organizationally Unique
  // Identifier) part and a NIC (Network Interface Controller) specific part.
  pcrecpp::RE mac_re("(.*?)("
                     "[0-9a-fA-F][0-9a-fA-F]:"
                     "[0-9a-fA-F][0-9a-fA-F]:"
                     "[0-9a-fA-F][0-9a-fA-F]):("
                     "[0-9a-fA-F][0-9a-fA-F]:"
                     "[0-9a-fA-F][0-9a-fA-F]:"
                     "[0-9a-fA-F][0-9a-fA-F])",
                     pcrecpp::RE_Options()
                     .set_multiline(true)
                     .set_dotall(true));

  string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  pcrecpp::StringPiece text(input);
  string pre_mac, oui, nic;
  while (mac_re.Consume(&text, &pre_mac, &oui, &nic)) {
    // Look up the MAC address in the hash.
    oui = StringToLowerASCII(oui);
    nic = StringToLowerASCII(nic);
    string mac = oui + ":" + nic;
    string replacement_mac = mac_addresses_[mac];
    if (replacement_mac.empty()) {
      // If not found, build up a replacement MAC address by generating a new
      // NIC part.
      int mac_id = mac_addresses_.size();
      replacement_mac = StringPrintf("%s:%02x:%02x:%02x",
                                     oui.c_str(),
                                     (mac_id & 0x00ff0000) >> 16,
                                     (mac_id & 0x0000ff00) >> 8,
                                     (mac_id & 0x000000ff));
      mac_addresses_[mac] = replacement_mac;
    }

    result += pre_mac;
    result += replacement_mac;
  }

  return result + text.as_string();
}

string AnonymizerTool::AnonymizeCustomPatterns(const string& input) {
  string anonymized = input;
  for (size_t i = 0; i < arraysize(kCustomPatterns); i++) {
    anonymized = AnonymizeCustomPattern(anonymized,
                                        kCustomPatterns[i],
                                        &custom_patterns_[i]);
  }
  return anonymized;
}

// static
string AnonymizerTool::AnonymizeCustomPattern(
    const string& input,
    const string& pattern,
    map<string, string>* identifier_space) {
  pcrecpp::RE re("(.*?)" + pattern,
                 pcrecpp::RE_Options()
                 .set_multiline(true)
                 .set_dotall(true));
  DCHECK_EQ(4, re.NumberOfCapturingGroups());

  string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  pcrecpp::StringPiece text(input);
  string pre_match, pre_matched_id, matched_id, post_matched_id;
  while (re.Consume(&text, &pre_match,
                    &pre_matched_id, &matched_id, &post_matched_id)) {
    string replacement_id = (*identifier_space)[matched_id];
    if (replacement_id.empty()) {
      replacement_id = IntToString(identifier_space->size());
      (*identifier_space)[matched_id] = replacement_id;
    }

    result += pre_match;
    result += pre_matched_id;
    result += replacement_id;
    result += post_matched_id;
  }
  result += text.as_string();
  return result;
}

}  // namespace debugd
