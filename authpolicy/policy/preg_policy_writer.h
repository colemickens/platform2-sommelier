// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_POLICY_PREG_POLICY_WRITER_H_
#define AUTHPOLICY_POLICY_PREG_POLICY_WRITER_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>

#include "authpolicy/policy/user_policy_encoder.h"

namespace policy {

// Helper class to write valid registry.pol ("PREG") files with the specified
// policy values. Useful to test preg parsing and encoding for in unit tests.
// See https://msdn.microsoft.com/en-us/library/aa374407(v=vs.85).aspx for a
// description of the file format.
class PRegPolicyWriter {
 public:
  // Creates a new writer using |registry_key| as key for mandatory policies.
  // Recommended policies are written under |registry_key| + "\Recommended".
  explicit PRegPolicyWriter(const std::string& registry_key);
  ~PRegPolicyWriter();

  // Appends a boolean policy value.
  void AppendBoolean(const char* policy_name,
                     bool value,
                     PolicyLevel level = POLICY_LEVEL_MANDATORY);

  // Appends an integer policy value.
  void AppendInteger(const char* policy_name,
                     uint32_t value,
                     PolicyLevel level = POLICY_LEVEL_MANDATORY);

  // Appends a string policy value.
  void AppendString(const char* policy_name,
                    const std::string& value,
                    PolicyLevel level = POLICY_LEVEL_MANDATORY);

  // Appends a string list policy value.
  void AppendStringList(const char* policy_name,
                        const std::vector<std::string>& values,
                        PolicyLevel level = POLICY_LEVEL_MANDATORY);

  // Writes the policy data to a file. Returns true on success.
  bool WriteToFile(const base::FilePath& path);

 private:
  // Starts a policy entry. Entries have the shape '[key;value;type;size;data]'.
  // This method writes '[key;value;type;size;'.
  void StartEntry(const std::string& key_name,
                  const std::string& value_name,
                  uint32_t data_type,
                  uint32_t data_size);

  // Ends a policy entry (writes ']'). The caller has to fill in the data
  // between StartEntry() and EndEntry().
  void EndEntry();

  // Appends a NULL terminated string to the internal buffer. Note that all
  // strings are written as char16s.
  void AppendNullTerminatedString(const std::string& str);

  // Appends an unsigned integer to the internal buffer.
  void AppendUnsignedInt(uint32_t value);

  // Appends a char16 to the internal buffer.
  void AppendChar16(base::char16 ch);

  // Returns the registry key that belongs to the given |level|.
  const std::string& GetKey(PolicyLevel level);

  std::string mandatory_key_;
  std::string recommended_key_;
  std::string buffer_;
  bool entry_started_ =
      false;  // Safety check that every StartEntry() is followed by EndEntry().
};

}  // namespace policy

#endif  // AUTHPOLICY_POLICY_PREG_POLICY_WRITER_H_
