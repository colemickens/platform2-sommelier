// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromiumos-wide-profiling/perf_option_parser.h"

#include "chromiumos-wide-profiling/compat/test.h"

namespace quipper {

TEST(PerfOptionParserTest, GoodRecord) {
  EXPECT_TRUE(ValidatePerfCommandLine(
      {"perf", "record"}));
  EXPECT_TRUE(ValidatePerfCommandLine(
      {"perf", "record", "-e", "cycles"}));
  EXPECT_TRUE(ValidatePerfCommandLine(
      {"perf", "record", "-e", "-$;(*^:,.Non-sense!"}));  // let perf reject it.
  EXPECT_TRUE(ValidatePerfCommandLine(
      {"perf", "record", "-a", "-e", "iTLB-misses", "-c", "1000003"}));
  EXPECT_TRUE(ValidatePerfCommandLine(
      {"perf", "record", "-a", "-e", "cycles", "-g", "-c", "4000037"}));
  EXPECT_TRUE(ValidatePerfCommandLine(
      {"perf", "record", "-a", "-e", "cycles", "-j", "any_call",
       "-c", "1000003"}));
}

TEST(PerfOptionParserTest, GoodStat) {
  EXPECT_TRUE(ValidatePerfCommandLine(
      {"perf", "stat", "-a", "-e", "cpu/mem-loads/", "-e", "cpu/mem-stores/"}));
}

// Options that control the output format should only be specified by quipper.
TEST(PerfOptionParserTest, BadRecord_OutputOptions) {
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "record", "-e", "cycles", "-v"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "record", "--verbose", "-e", "cycles"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "record", "-q", "-e", "cycles"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "record", "-e", "cycles", "--quiet"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "record", "-e", "cycles", "-m", "512"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "record", "-e", "cycles", "--mmap-pages", "512"}));
}

TEST(PerfOptionParserTest, BadRecord_BannedOptions) {
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "record", "-e", "cycles", "-D"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "record", "-e", "cycles", "-D", "10"}));
}

// Options that control the output format should only be specified by quipper.
TEST(PerfOptionParserTest, BadStat_OutputOptions) {
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "stat", "-e", "cycles", "-v"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "stat", "--verbose", "-e", "cycles"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "stat", "-q", "-e", "cycles"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "stat", "-e", "cycles", "--quiet"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "stat", "-e", "cycles", "-x", "::"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "stat", "-e", "cycles", "--field-separator", ","}));
}

TEST(PerfOptionParserTest, BadStat_BannedOptions) {
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "stat", "--pre", "rm -rf /"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "stat", "--post", "rm -rf /"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "stat", "-d"}));
  EXPECT_FALSE(ValidatePerfCommandLine(
      {"perf", "stat", "--log-fd", "4"}));
}

TEST(PerfOptionParserTest, DontAllowOtherPerfSubcommands) {
  EXPECT_FALSE(ValidatePerfCommandLine({"perf", "list"}));
  EXPECT_FALSE(ValidatePerfCommandLine({"perf", "report"}));
  EXPECT_FALSE(ValidatePerfCommandLine({"perf", "trace"}));
}

// Unsafe command lines for either perf command.
TEST(PerfOptionParserTest, Ugly) {
  for (const string &subcmd : { "record", "stat" }) {
    EXPECT_FALSE(ValidatePerfCommandLine(
        {"perf", subcmd, "rm", "-rf", "/"}));
    EXPECT_FALSE(ValidatePerfCommandLine(
        {"perf", subcmd, "--", "rm", "-rf", "/"}));
    EXPECT_FALSE(ValidatePerfCommandLine(
        {"perf", subcmd, "-e", "cycles", "rm", "-rf", "/"}));
    EXPECT_FALSE(ValidatePerfCommandLine(
        {"perf", subcmd, "-e", "cycles", "-o", "/root/haha.perf.data"}));
  }
}

}  // namespace quipper

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
