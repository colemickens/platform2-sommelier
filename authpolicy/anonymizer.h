// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_ANONYMIZER_H_
#define AUTHPOLICY_ANONYMIZER_H_

#include <map>
#include <string>

#include <base/logging.h>
#include <base/macros.h>

namespace authpolicy {

// Log anonymizer that performs simple search&replace operations on log strings.
// This approach is taken instead of regex replacements since Samba and kinit
// are pretty much black boxes and finding regular expressions to match all
// occurances of sensitive data in their logs would be very cumbersome and
// insecure because we cannot guarantee that all code paths are hit. This
// sledgehammer approach is more secure.
class Anonymizer {
 public:
  Anonymizer();

  // Causes Process() to replace |string_to_replace| by |replacement|.
  void SetReplacement(const std::string& string_to_replace,
                      const std::string& replacement);

  // Causes Process() to search for "|search_keyword|: <value><newline>" and to
  // set the replacement <value> -> |replacement| before all replacements are
  // applied to the input string. This is useful for logging results from
  // searching sensitive data (e.g. net ads search for user names). It solves
  // the chicken-egg-problem where one would usually like to log results before
  // parsing them (or in case parsing fails), but replacements cannot be set
  // before the results are parsed.
  void ReplaceSearchArg(const std::string& search_keyword,
                        const std::string& replacement);

  // Resets all calls to ReplaceSearchArg(), but keeps the replacements set by
  // a call to Process() in between. Should be done after a search log has been
  // logged.
  void ResetSearchArgReplacements();

  // Runs the anonymizer on the given |input|, replacing all strings with their
  // given replacement. Returns the anonymized string.
  std::string Process(const std::string& input);

 private:
  // Maps string-to-replace to their replacement.
  std::map<std::string, std::string> replacements_;

  // Maps search keywords to the replacement of the search value.
  std::map<std::string, std::string> search_replacements_;

  DISALLOW_COPY_AND_ASSIGN(Anonymizer);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_ANONYMIZER_H_
