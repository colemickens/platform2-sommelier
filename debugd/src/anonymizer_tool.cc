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

AnonymizerTool::AnonymizerTool() {}

AnonymizerTool::~AnonymizerTool() {}

string AnonymizerTool::Anonymize(const string& input) {
  return AnonymizeMACAddresses(input);
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

};  // namespace debugd
