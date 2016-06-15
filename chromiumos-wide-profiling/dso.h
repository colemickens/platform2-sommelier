// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMIUMOS_WIDE_PROFILING_DSO_H_
#define CHROMIUMOS_WIDE_PROFILING_DSO_H_

#include "chromiumos-wide-profiling/compat/string.h"
#include "chromiumos-wide-profiling/data_reader.h"

namespace quipper {

void InitializeLibelf();
bool ReadElfBuildId(string filename, string* buildid);

// Read buildid from /sys/module/<module_name>/notes/.note.gnu.build-id
bool ReadModuleBuildId(string module_name, string* buildid);
// Read builid from Elf note data section.
bool ReadBuildIdNote(DataReader* data, string* buildid);

// Is |name| match one of the things reported by the kernel that is known
// not to be a kernel module?
bool IsKernelNonModuleName(string name);

}  // namespace quipper

#endif  // CHROMIUMOS_WIDE_PROFILING_DSO_H_
