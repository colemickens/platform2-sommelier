/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string>

#include <base/json/json_reader.h>
#include <base/values.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "runtime_probe/probe_function.h"
#include "runtime_probe/probe_result_checker.h"
#include "runtime_probe/probe_statement.h"

namespace runtime_probe {

class MockProbeFunction : public ProbeFunction {
 public:
  MOCK_CONST_METHOD0(Eval, DataType());
};

TEST(ProbeStatementTest, TestEval) {
  ProbeStatement probe_statement;

  // Set up |expect_|
  auto expect_string = R"({
    "expected_field": [true, "str"]
  })";
  auto expect =
      base::DictionaryValue::From(base::JSONReader::Read(expect_string));

  probe_statement.expect_ = ProbeResultChecker::FromDictionaryValue(*expect);

  // Set up |eval_|
  auto mock_eval = std::make_unique<MockProbeFunction>();

  base::DictionaryValue good_result;
  good_result.SetString("expected_field", "expected");
  good_result.SetString("optional_field", "optional");

  // bad_result is empty, which doesn't have expected field
  base::DictionaryValue bad_result;
  bad_result.SetString("optional_field", "optional");

  EXPECT_CALL(*mock_eval, Eval())
      .WillOnce(
          ::testing::Return(ProbeFunction::DataType{good_result, bad_result}))
      .WillOnce(::testing::Return(ProbeFunction::DataType{good_result}));

  probe_statement.eval_ = std::move(mock_eval);

  // Test twice, both invocations should only return |good_result|.
  for (auto i = 0; i < 2; i++) {
    auto results = probe_statement.Eval();
    ASSERT_EQ(results.size(), 1);

    std::string str_value;
    ASSERT_TRUE(results[0].GetString("expected_field", &str_value));
    ASSERT_EQ(str_value, "expected");

    ASSERT_TRUE(results[0].GetString("optional_field", &str_value));
    ASSERT_EQ(str_value, "optional");
  }
}

}  // namespace runtime_probe
