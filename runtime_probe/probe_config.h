/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef RUNTIME_PROBE_PROBE_CONFIG_H_
#define RUNTIME_PROBE_PROBE_CONFIG_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/values.h>
#include <gtest/gtest.h>

#include "runtime_probe/component_category.h"

namespace runtime_probe {

class ProbeConfig {
  /* Holds a probe config.
   *
   * The input will be in JSON format with following schema::
   *   {
   *     <category:string>: {
   *       <component_name:string>: <statement:ProbeStatement>,
   *       ...
   *     }
   *   }
   */

 public:
  static std::unique_ptr<ProbeConfig> FromDictionaryValue(
      const base::DictionaryValue& dict_value);

  /* Evaluates the probe config.
   *
   * @param category: specifies the components to probe.
   * @return DictionaryValue with the following format:
   *   {
   *     <category:string>: [
   *       {
   *         "name": <component_name:string>,
   *         "values": <probed_values of ProbeStatement>,
   *         "information": <information of ProbeStatement>
   *       }
   *     ]
   *   }
   */
  std::unique_ptr<base::DictionaryValue> Eval(
      const std::vector<std::string>& category) const;

  /* Evaluates the probe config.
   *
   * This is the same as calling `this->eval({keys of category_})`.
   */
  std::unique_ptr<base::DictionaryValue> Eval() const;

 private:
  /* Must call `FromDictionaryValue()` to create an instance. */
  ProbeConfig() = default;

  std::map<std::string, std::unique_ptr<ComponentCategory>> category_;

  FRIEND_TEST(ProbeConfigTest, LoadConfig);
};

} /* namespace runtime_probe */

#endif  // RUNTIME_PROBE_PROBE_CONFIG_H_
