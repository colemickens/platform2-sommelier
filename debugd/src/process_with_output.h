// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PROCESS_WITH_OUTPUT_H
#define PROCESS_WITH_OUTPUT_H

#include <string>
#include <vector>

#include <base/file_path.h>
#include <chromeos/process.h>

namespace debugd {

// @brief Represents a process whose output can be collected.
//
// The process must be Run() to completion before its output can be collected.
class ProcessWithOutput : public chromeos::ProcessImpl {
 public:
  ProcessWithOutput();
  ~ProcessWithOutput();
  bool Init();
  bool GetOutput(std::string* output);
  bool GetOutputLines(std::vector<std::string>* output);
 private:
  FilePath outfile_path_;
  FILE *outfile_;
};

};  // namespace debugd

#endif  // PROCESS_WITH_OUTPUT_H
