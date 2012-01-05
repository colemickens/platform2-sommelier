// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "process_with_output.h"

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/string_split.h>

namespace debugd {

ProcessWithOutput::ProcessWithOutput() : outfile_(NULL) { }
ProcessWithOutput::~ProcessWithOutput() {
  if (outfile_)
    fclose(outfile_);
  if (!outfile_path_.empty())
    file_util::Delete(outfile_path_, false);  // not recursive
}

bool ProcessWithOutput::Init() {
  outfile_ = file_util::CreateAndOpenTemporaryFile(&outfile_path_);
  if (!outfile_)
    return false;
  // We can't just RedirectOutput to the file we just created, since
  // RedirectOutput uses O_CREAT | O_EXCL to open the target file (i.e., it'll
  // fail if the file already exists). We can't CreateTemporaryFile() and then
  // use that filename, since we'd have to remove it before using
  // RedirectOutput, which exposes us to a /tmp race. Instead, bind outfile_'s
  // fd to the subprocess's stdout and stderr.
  BindFd(fileno(outfile_), STDOUT_FILENO);
  BindFd(fileno(outfile_), STDERR_FILENO);
  return true;
}

bool ProcessWithOutput::GetOutputLines(std::vector<std::string>* output) {
  std::string contents;
  if (!file_util::ReadFileToString(outfile_path_, &contents))
    return false;
  base::SplitString(contents, '\n', output);
  return true;
}

bool ProcessWithOutput::GetOutput(std::string* output) {
  return file_util::ReadFileToString(outfile_path_, output);
}

};  // namespace debugd
