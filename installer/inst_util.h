// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INST_UTIL
#define INST_UTIL

#include <string>
#include <vector>

#define RUN_OR_RETURN_FALSE(_x)                                 \
  do {                                                          \
    if (RunCommand(_x) != 0) return false;                      \
  } while (0)

std::string StringPrintf(const std::string& format, ...);

void SplitString(const std::string& str,
                 char split,
                 std::vector<std::string>* output);

// This is a place holder to invoke the backing scripts. Once all scripts have
// been rewritten as library calls this command should be deleted.
int RunCommand(const std::string& command);

std::string LsbReleaseValue(const std::string& file,
                            const std::string& key);

bool VersionLess(const std::string& left,
                 const std::string& right);

std::string GetBlockDevFromPartitionDev(const std::string& partition_dev);

int GetPartitionFromPartitionDev(const std::string& partition_dev);

std::string MakePartitionDev(const std::string& partition_dev,
                             int partition);

// Convert /blah/file to /blah
std::string Dirname(const std::string& filename);

// rm *pack from /dirname
bool RemovePackFiles(const std::string& dirname);

// Create an empty file
bool Touch(const std::string& filename);

// See bug chromium-os:11517. This fixes an old FS corruption problem.
bool R10FileSystemPatch(const std::string& dev_name);

// Mark ext2 (3 or 4???) filesystem RW
bool MakeFileSystemRw(const std::string& dev_name, bool rw);

// hdparm -r 1 /device
bool MakeDeviceReadOnly(const std::string& dev_name);

// Conveniently invoke the external dump_kernel_config library
std::string DumpKernelConfig(const std::string& kernel_dev);

// ExtractKernelNamedArg(DumpKernelConfig(..), "root") -> /dev/dm-0
// This understands quoted values. dm -> "a b c, foo=far"
std::string ExtractKernelArg(const std::string& kernel_config,
                               const std::string& tag);

bool CopyVerityHashes(const std::string& hash_file,
                      const std::string& dev_name,
                      int offset_sectors);

#endif // INST_UTIL
