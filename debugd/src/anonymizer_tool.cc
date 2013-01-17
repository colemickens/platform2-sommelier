// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "anonymizer_tool.h"

#include <pcrecpp.h>

#include <base/string_util.h>
#include <base/stringprintf.h>

using base::StringPrintf;
using std::string;

namespace debugd {

namespace {

struct CustomPatternInfo {
  const char* const tag;
  const char* const pattern;
};

// This array defines custom patterns to match and anonymize. Every match of
// |pattern| will be replaced by a "|tag|-|id|" string, where |id| is the
// incremental instance identifier of the matched substring. Every different
// |tag| defines a separate instance identifier space.
const CustomPatternInfo kCustomPatterns[] = {
  { "cell-id", "\\bCell ID: '[0-9a-fA-F]+'" },
  { "loc-area-code", "\\bLocation area code: '[0-9a-fA-F]+'" },
};

}  // namespace

AnonymizerTool::AnonymizerTool() {}

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
                                        kCustomPatterns[i].tag,
                                        kCustomPatterns[i].pattern);
  }
  return anonymized;
}

string AnonymizerTool::AnonymizeCustomPattern(
    const string& input, const string& tag, const string& pattern) {
  pcrecpp::RE re("(.*?)(" + pattern + ")",
                 pcrecpp::RE_Options()
                 .set_multiline(true)
                 .set_dotall(true));

  string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  pcrecpp::StringPiece text(input);
  string pre_match, match;
  while (re.Consume(&text, &pre_match, &match)) {
    string replacement = custom_patterns_[tag][match];
    if (replacement.empty()) {
      int id = custom_patterns_[tag].size();
      replacement = StringPrintf("%s-%d", tag.c_str(), id);
      custom_patterns_[tag][match] = replacement;
    }

    result += pre_match;
    result += replacement;
  }

  return result + text.as_string();
}

};  // namespace debugd
