// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_PROCESS_WITH_OUTPUT_H_
#define DEBUGD_SRC_PROCESS_WITH_OUTPUT_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>

#include "debugd/src/sandboxed_process.h"

namespace debugd {

// @brief Represents a process whose output can be collected.
//
// The process must be Run() to completion before its output can be collected.
class ProcessWithOutput : public SandboxedProcess {
 public:
  ProcessWithOutput();
  ~ProcessWithOutput() override;
  bool Init() override;
  bool GetOutput(std::string* output);
  bool GetOutputLines(std::vector<std::string>* output);

 private:
  base::FilePath outfile_path_;
  FILE *outfile_;
};

}  // namespace debugd

#endif  // DEBUGD_SRC_PROCESS_WITH_OUTPUT_H_
