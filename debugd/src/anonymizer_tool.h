// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANONYMIZER_TOOL_H_
#define ANONYMIZER_TOOL_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>

namespace debugd {

class AnonymizerTool {
 public:
  AnonymizerTool();
  ~AnonymizerTool();

  // Returns an anonymized version of |input|. PII-sensitive data (such as MAC
  // addresses) in |input| is replaced with unique identifiers.
  std::string Anonymize(const std::string& input);

 private:
  friend class AnonymizerToolTest;

  std::string AnonymizeMACAddresses(const std::string& input);
  std::string AnonymizeCustomPatterns(const std::string& input);
  static std::string AnonymizeCustomPattern(
      const std::string& input,
      const std::string& pattern,
      std::map<std::string, std::string>* identifier_space);

  std::map<std::string, std::string> mac_addresses_;
  std::vector<std::map<std::string, std::string> > custom_patterns_;

  DISALLOW_COPY_AND_ASSIGN(AnonymizerTool);
};

}  // namespace debugd

#endif  // ANONYMIZER_TOOL_H_
