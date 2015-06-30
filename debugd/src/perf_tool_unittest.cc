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
  void set_model_name(const std::string& model_name) {
    model_name_ = model_name;
  }
  void set_intel_family_model(const std::string& family_and_model) {
    intel_family_model_ = family_and_model;
  }
};

// Tests the CPU odds file lookup against various CPU architecture and CPU model
// name strings.
TEST(PerfToolTest, TestCPUOddsFileLookup) {
  // Use a struct to define the expected outputs for each set of inputs.
  const struct {
    // Inputs.
    const char* arch;
    const char* model_name;
    const char* intel_model;  // family and model
    // Expected output.
    const char* filename;
  } kOddsFileTestCases[] = {
    // 64-bit x86.
    { .arch = "x86_64",
      .model_name = "Intel(R) Celeron(R) 2955U @ 1.40GHz",
      .intel_model = "06_45",  // Haswell
      .filename = "/etc/perf_commands/x86_64/Haswell.txt" },
    { .arch = "x86_64",
      .model_name = "Intel(R) Core(TM) i5-2467M CPU @ 1.60GHz",
      .intel_model = "06_2A",  // SandyBridge
      .filename = "/etc/perf_commands/x86_64/SandyBridge.txt" },
    { .arch = "x86_64",
      .model_name = "Intel(R) Core(TM) i5-3427U CPU @ 1.80GHz",
      .intel_model = "06_3A",  // IvyBridge
      .filename = "/etc/perf_commands/x86_64/IvyBridge.txt" },
    { .arch = "x86_64",
      .model_name = "Intel(R) Celeron(R) CPU 867 @ 1.30GHz",
      .intel_model = "06_2A",  // SandyBridge
      .filename = "/etc/perf_commands/x86_64/SandyBridge.txt" },
    { .arch = "x86_64",
      .model_name = "Intel(R) Celeron(R) CPU 847 @ 1.10GHz",
      .intel_model = "06_2A",  // SandyBridge
      .filename = "/etc/perf_commands/x86_64/SandyBridge.txt" },
    { .arch = "x86_64",
      .model_name = "Intel(R) Celeron(R) CPU N2830 @ 2.13GHz",
      .intel_model = "06_37",  // Silvermont
      .filename = "/etc/perf_commands/x86_64/default.txt" },

    // 32-bit x86.
    { .arch = "i686",
      .model_name = "Intel(R) Atom(TM) CPU N455 @ 1.66GHz",
      .intel_model = "06_1C",  // Bonnell
      .filename = "/etc/perf_commands/i686/default.txt" },
    { .arch = "i686",
      .model_name = "Intel(R) Atom(TM) CPU N570 @ 1.66GHz",
      .intel_model = "06_1C",  // Bonnell
      .filename = "/etc/perf_commands/i686/default.txt" },
    { .arch = "i686",
      .model_name = "Intel(R) Pentium(TM) M 1.3 @ 1.3GHz",
      .intel_model = "06_0D",  // Dothan (probably)
      .filename = "/etc/perf_commands/i686/default.txt" },
    { .arch = "i686",
      .model_name = "Intel(R) Pentium(TM) M 705 @ 1.5GHz",
      .intel_model = "06_09",  // Banias
      .filename = "/etc/perf_commands/i686/default.txt" },

    // ARMv7
    { .arch = "armv7l",
      .model_name = "ARMv7 Processor rev 3 (v7l)",
      .intel_model = nullptr,
      .filename = "/etc/perf_commands/armv7l/default.txt" },
    { .arch = "armv7l",
      .model_name = "ARMv7 Processor rev 4 (v7l)",
      .intel_model = nullptr,
      .filename = "/etc/perf_commands/armv7l/default.txt" },

    // Misc.
    { .arch = "MIPS",
      .model_name = "blah",
      .intel_model = nullptr,
      .filename = "/etc/perf_commands/unknown.txt" },
    { .arch = "AVR",
      .model_name = "blah",
      .intel_model = nullptr,
      .filename = "/etc/perf_commands/unknown.txt" },
    { .arch = "MC68000",
      .model_name = "blah",
      .intel_model = nullptr,
      .filename = "/etc/perf_commands/unknown.txt" },
  };

  for (const auto& test_case : kOddsFileTestCases) {
    // Set custom CPU info inputs.
    MockCPUInfoReader cpu_info;
    cpu_info.set_arch(test_case.arch);
    cpu_info.set_model_name(test_case.model_name);
    if (test_case.intel_model)
      cpu_info.set_intel_family_model(test_case.intel_model);

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
