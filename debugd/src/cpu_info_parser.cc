// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cpu_info_parser.h"

#include <fstream>
#include <string>

#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace {

// The filename that contains CPU information.
const char kCPUInfoFilename[] = "/proc/cpuinfo";

// The delimiter used in the CPU information filename to separate keys and
// values.
const char kCPUInfoKeyValueDelimiter = ':';

}  // namespace

namespace debugd {

CPUInfoParser::CPUInfoParser() : cpu_info_filename_(kCPUInfoFilename) {}

bool CPUInfoParser::GetKey(const std::string& key, std::string* value) {
  std::ifstream infile(cpu_info_filename_.c_str());
  CHECK(infile.good());
  std::string line;
  while (std::getline(infile, line)) {
    std::vector<std::string> tokens;
    base::SplitString(line, kCPUInfoKeyValueDelimiter, &tokens);
    if (!tokens.size())
      continue;
    base::TrimWhitespace(tokens[0], base::TRIM_ALL, &tokens[0]);
    if (tokens[0] == key) {
      *value = tokens[1];
      return true;
    }
  }
  return false;
}

}  // namespace debugd
