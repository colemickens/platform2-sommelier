/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef RUNTIME_PROBE_PROBE_RESULT_CHECKER_H_
#define RUNTIME_PROBE_PROBE_RESULT_CHECKER_H_

#include <map>
#include <memory>
#include <string>

#include <base/values.h>
#include <gtest/gtest.h>

namespace runtime_probe {

class FieldConverter {
 public:
  enum class ReturnCode {
    OK = 0,
    FIELD_NOT_FOUND = 1,
    /* Failed to convert the field */
    INCOMPATIBLE_VALUE = 2,
  };

  /* Try to find |field_name| in dict_value, and convert it to expected type.
   *
   * @return |ReturnCode| to indicate success or reason of failure.
   */
  virtual ReturnCode Convert(const std::string& field_name,
                             base::DictionaryValue* const dict_value) const = 0;

  virtual std::string ToString() const = 0;

  virtual ~FieldConverter() = default;
};

/* Convert a field to string.  */
class StringFieldConverter : public FieldConverter {
 public:
  ReturnCode Convert(const std::string& field_name,
                     base::DictionaryValue* const dict_value) const override;

  std::string ToString() const override { return "StringFieldConverter()"; }
};

/* Convert a field to integer.
 *
 * However, heximal value is not allowed, please use |HexFieldConverter|
 * instead.
 */
class IntegerFieldConverter : public FieldConverter {
 public:
  ReturnCode Convert(const std::string& field_name,
                     base::DictionaryValue* const dict_value) const override;

  std::string ToString() const override { return "IntegerFieldConverter()"; }
};

/* Convert a hex string field to integer.
 *
 * If the original field is string, this class assumes it is base 16.
 * Otherwise, if the field is already a number (int or double), the behavior wil
 * be identical to |IntegerFieldConverter|.
 */
class HexFieldConverter : public FieldConverter {
 public:
  ReturnCode Convert(const std::string& field_name,
                     base::DictionaryValue* const dict_value) const override;

  std::string ToString() const override { return "HexFieldConverter()"; }
};

/* Convert a field to double.  */
class DoubleFieldConverter : public FieldConverter {
 public:
  ReturnCode Convert(const std::string& field_name,
                     base::DictionaryValue* const dict_value) const override;

  std::string ToString() const override { return "DoubleFieldConverter()"; }
};

/* Holds |expect| attribute of a |ProbeStatement|.
 *
 * |expect| attribute should be a |DictionaryValue| with following format:
 * {
 *   <key_of_probe_result>: [<required:bool>, <expected_type:string>,
 *                           <optional_validate_rule:string>]
 * }
 *
 * Currently, we support the following expected types:
 * - "int"  (use |IntegerFieldConverter|)
 * - "hex"  (use |HexFieldConverter|)
 * - "double"  (use |DoubleFieldConverter|)
 * - "str"  (use |StringFieldConverter|)
 *
 * |ProbeResultChecker| will first try to convert each field to |expected_type|.
 * Then, if |optional_validate_rule| is given, will check if converted value
 * match the rule.
 *
 * TODO(b/121354690): Handle |optional_validate_rule|.
 */
class ProbeResultChecker {
 public:
  static std::unique_ptr<ProbeResultChecker> FromDictionaryValue(
      const base::DictionaryValue& dict_value);

  /* Apply |expect| rules to |probe_result|
   *
   * @return |true| if all required fields are converted successfully.
   */
  bool Apply(base::DictionaryValue* probe_result) const;

 private:
  std::map<std::string, std::unique_ptr<FieldConverter>> required_fields_;
  std::map<std::string, std::unique_ptr<FieldConverter>> optional_fields_;

  FRIEND_TEST(ProbeResultCheckerTest, TestFromDictionaryValue);
};

}  // namespace runtime_probe

#endif  // RUNTIME_PROBE_PROBE_RESULT_CHECKER_H_
