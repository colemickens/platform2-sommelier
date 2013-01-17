// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANONYMIZER_TOOL_H
#define ANONYMIZER_TOOL_H

#include <map>
#include <string>

#include <base/basictypes.h>

namespace debugd {

class AnonymizerTool {
 public:
  AnonymizerTool();
  virtual ~AnonymizerTool();

  // Returns an anonymized version of |input|. PII-sensitive data (such as MAC
  // addresses) in |input| is replaced with unique identifiers.
  std::string Anonymize(const std::string& input);

 private:
  friend class AnonymizerToolTest;

  std::string AnonymizeMACAddresses(const std::string& input);
  std::string AnonymizeCustomPatterns(const std::string& input);
  std::string AnonymizeCustomPattern(const std::string& input,
                                     const std::string& tag,
                                     const std::string& pattern);

  std::map<std::string, std::string> mac_addresses_;

  // Maps tag -> (match -> replacement).
  std::map<std::string, std::map<std::string, std::string> > custom_patterns_;

  DISALLOW_COPY_AND_ASSIGN(AnonymizerTool);
};

};  // namespace debugd

#endif  // ANONYMIZER_TOOL_H
