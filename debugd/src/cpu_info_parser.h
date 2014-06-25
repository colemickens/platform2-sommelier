// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CPU_INFO_PARSER_H_
#define CPU_INFO_PARSER_H_

#include <string>

namespace debugd {

// CPUInfoParser is a class that can be used to parse /proc/cpuinfo and gather
// data from it.
//
// Example:
//
// CPUInfoParser cpu_info_parser;
// std::string cpu_model_name;
// cpu_info_parser.GetKey("model name", &cpu_model_name);
// cpu_model_name should now contain something like "Intel (R) Celeron(R) CPU".
class CPUInfoParser {
 public:
  // Constructor.
  CPUInfoParser();

  // Get particular key from the cpu information file. Note that this function
  // reads the CPU information file every time it is called. This is because the
  // contents of the file can change with time.
  bool GetKey(const std::string& key, std::string* value);

  // Setter for cpu_info_filename_.
  void set_cpu_info_filename(const std::string& filename) {
    cpu_info_filename_ = filename;
  }

  // Getter for cpu_info_filename_.
  std::string cpu_info_filename() const {
    return cpu_info_filename_;
  }

 private:
  // The entire /proc/cpuinfo contents.
  std::string contents_;

  // The name of the cpu info file.
  std::string cpu_info_filename_;
};

}  // namespace debugd

#endif  // CPU_INFO_PARSER_H_

