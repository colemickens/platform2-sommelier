// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/perf_tool.h"

#include <base/memory/scoped_ptr.h>
#include <base/strings/string_util.h>
#include <gtest/gtest.h>

#include "debugd/src/random_selector.h"

namespace debugd {

// Mock version of Random selector that just stores the odds file path, without
// reading from it. This allows us to test the file path strings without needing
// them to exist.
class MockRandomSelector : public RandomSelector {
 public:
  // Override the SetOddsFromFile() function to simply store the filename.
  void SetOddsFromFile(const std::string& filename) override {
    filename_ = filename;
  }

  const std::string& filename() const {
    return filename_;
  }

 private:
  std::string filename_;
};

// Mock version of CPUInfoReader that lets us specify custom info.
class MockCPUInfoReader : public PerfTool::CPUInfoReader {
 public:
  // Explicitly specify mutators instead of passing these in as constructor
  // arguments. This avoids accidentally reversing |arch| and |model|.
  void set_arch(const std::string& arch) {
    arch_ = arch;
  }
  void set_model(const std::string& model) {
    model_ = model;
  }
};

// Tests the CPU odds file lookup against various CPU architecture and CPU model
// name strings.
TEST(PerfToolTest, TestCPUOddsFileLookup) {
  // Use a struct to define the expected outputs for each set of inputs.
  struct OddsFileTestCase {
    // Inputs.
    const char* arch;
    const char* model;
    // Expected output.
    const char* filename;
  } kOddsFileTestCases[] = {
    // 64-bit x86.
    { .arch = "x86_64",
      .model = "Intel(R) Celeron(R) 2955U @ 1.40GHz",
      .filename = "/etc/perf_commands/x86_64/celeron-2955u.txt" },
    { .arch = "x86_64",
      .model = "Intel(R) Core(TM) i5-2467M CPU @ 1.60GHz",
      .filename = "/etc/perf_commands/x86_64/default.txt" },
    { .arch = "x86_64",
      .model = "Intel(R) Core(TM) i5-3427U CPU @ 1.80GHz",
      .filename = "/etc/perf_commands/x86_64/default.txt" },
    { .arch = "x86_64",
      .model = "Intel(R) Celeron(R) CPU 867 @ 1.30GHz",
      .filename = "/etc/perf_commands/x86_64/default.txt" },
    { .arch = "x86_64",
      .model = "Intel(R) Celeron(R) CPU 847 @ 1.10GHz",
      .filename = "/etc/perf_commands/x86_64/default.txt" },

    // 32-bit x86.
    { .arch = "i686",
      .model = "Intel(R) Atom(TM) CPU N455 @ 1.66GHz",
      .filename = "/etc/perf_commands/i686/default.txt" },
    { .arch = "i686",
      .model = "Intel(R) Atom(TM) CPU N570 @ 1.66GHz",
      .filename = "/etc/perf_commands/i686/default.txt" },
    { .arch = "i686",
      .model = "Intel(R) Pentium(TM) M 1.3 @ 1.3GHz",
      .filename = "/etc/perf_commands/i686/default.txt" },
    { .arch = "i686",
      .model = "Intel(R) Pentium(TM) M 705 @ 1.5GHz",
      .filename = "/etc/perf_commands/i686/default.txt" },

    // ARMv7
    { .arch = "armv7l",
      .model = "ARMv7 Processor rev 3 (v7l)",
      .filename = "/etc/perf_commands/armv7l/default.txt" },
    { .arch = "armv7l",
      .model = "ARMv7 Processor rev 4 (v7l)",
      .filename = "/etc/perf_commands/armv7l/default.txt" },

    // Misc.
    { .arch = "MIPS",
      .model = "blah",
      .filename = "/etc/perf_commands/unknown.txt" },
    { .arch = "AVR",
      .model = "blah",
      .filename = "/etc/perf_commands/unknown.txt" },
    { .arch = "MC68000",
      .model = "blah",
      .filename = "/etc/perf_commands/unknown.txt" },
  };

  for (size_t i = 0; i < arraysize(kOddsFileTestCases); ++i) {
    const OddsFileTestCase& test_case = kOddsFileTestCases[i];

    // Set custom CPU info inputs.
    MockCPUInfoReader cpu_info;
    cpu_info.set_arch(test_case.arch);
    cpu_info.set_model(test_case.model);

    // Instantiate a PerfTool with custom inputs.
    // |random_selector| will be deleted by PerfTool's destructor.
    MockRandomSelector* random_selector = new MockRandomSelector;
    std::unique_ptr<MockRandomSelector> random_selector_ptr(random_selector);
    PerfTool perf_tool(cpu_info, random_selector_ptr.release());

    // Check if PerfTool has passed the correct path to our MockRandomSelector.
    EXPECT_EQ(test_case.filename, random_selector->filename());
  }
}

}  // namespace debugd
