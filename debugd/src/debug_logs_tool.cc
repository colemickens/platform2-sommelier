// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/debug_logs_tool.h"

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <brillo/process.h>

namespace debugd {

namespace {

constexpr char kCat[] = "/bin/cat";
constexpr char kArcBugreportPipe[] = "/run/arc/bugreport/pipe";
constexpr char kArcBugreportFile[] = "arc-bugreport.txt";

constexpr char kTar[] = "/bin/tar";
constexpr char kSystemLogs[] = "/var/log";

// Executes 'bugreport' command in the container through the pipe, then writes
// the output to |arc_bugreport_file|. When the container is not running,
// the function returns immediately without writing anything.
void WriteArcBugreport(const base::FilePath& arc_bugreport_file) {
  brillo::ProcessImpl p;
  p.AddArg(kCat);
  p.AddArg(kArcBugreportPipe);
  p.RedirectOutput(arc_bugreport_file.value());
  p.Run();
}

}  // namespace

void DebugLogsTool::GetDebugLogs(bool is_compressed,
                                 const base::ScopedFD& fd) {
  base::ScopedTempDir arc_temp_dir;

  // Create a temporary file and write ARC log to the file if ARC is running.
  if (base::PathExists(base::FilePath(kArcBugreportPipe))) {
    if (arc_temp_dir.CreateUniqueTempDir())
      WriteArcBugreport(arc_temp_dir.GetPath().Append(kArcBugreportFile));
    else
      PLOG(WARNING) << "Failed to create a temporary directory";
  }

  brillo::ProcessImpl p;
  p.AddArg(kTar);
  p.AddArg("-c");
  if (is_compressed)
    p.AddArg("-z");

  if (arc_temp_dir.IsValid()) {
    p.AddArg("-C");
    p.AddArg(arc_temp_dir.GetPath().value());
    p.AddArg(kArcBugreportFile);
  }
  p.AddArg(kSystemLogs);
  p.BindFd(fd.get(), STDOUT_FILENO);
  p.Run();
}

}  // namespace debugd
