// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>

#include <base/files/file_path.h>
#include <gtest/gtest.h>

#include "runtime_probe/statement_parser.h"

namespace runtime_probe {

namespace {
std::string GetTestDataPath(const std::string& file_name) {
  const char* src_dir = getenv("SRC");
  CHECK(src_dir != nullptr);
  return base::FilePath(src_dir)
      .Append("testdata")
      .Append(file_name)
      .MaybeAsASCII();
}
}  // namespace

TEST(StatementParserTest, ReadFromFile) {
  // Random file path. Should return nullptr
  EXPECT_EQ(ParseProbeStatement("/random/file/path"), nullptr);
  // File is not in JSON format. Should return nullptr
  EXPECT_EQ(ParseProbeStatement(GetTestDataPath("test.txt")), nullptr);
  // testdata/probe_statement.json is a valid JSON file. Should read
  // successfully and return non-null std::unique_ptr<DictionaryValue>
  EXPECT_NE(ParseProbeStatement(GetTestDataPath("probe_statement.json")),
            nullptr);
}

}  // namespace runtime_probe
