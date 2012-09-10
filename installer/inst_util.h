// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INST_UTIL_H_
#define INST_UTIL_H_

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

bool ReadFileToString(const std::string& path, std::string* contents);

bool WriteStringToFile(const std::string& contents, const std::string& path);

// Copies a single file.
bool CopyFile(const std::string& from_path, const std::string& to_path);

bool LsbReleaseValue(const std::string& file,
                     const std::string& key,
                     std::string* result);

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

// Replace the first instance of pattern in the file with value.
bool ReplaceInFile(const std::string& pattern,
                   const std::string& value,
                   const std::string& path);

// See bug chromium-os:11517. This fixes an old FS corruption problem.
bool R10FileSystemPatch(const std::string& dev_name);

// Mark ext2 (3 or 4???) filesystem RW
bool MakeFileSystemRw(const std::string& dev_name, bool rw);

// hdparm -r 1 /device
bool MakeDeviceReadOnly(const std::string& dev_name);

// Conveniently invoke the external dump_kernel_config library
std::string DumpKernelConfig(const std::string& kernel_dev);

// ExtractKernelNamedArg(DumpKernelConfig(..), "root") -> /dev/dm-0
// This understands quoted values. dm -> "a b c, foo=far" (strips quotes)
std::string ExtractKernelArg(const std::string& kernel_config,
                             const std::string& tag);

// Take a kernel style argument list and modify a single argument
// value. Quotes will be added to the value if needed.
bool SetKernelArg(const std::string& tag,
                  const std::string& value,
                  std::string* kernel_config);

// IsReadonly determines if the name devices should be treated as
// read-only. This is based on the device name being prefixed with
// "/dev/dm". This catches both cases where verity may be /dev/dm-0
// or /dev/dm-1.
bool IsReadonly(const std::string& device);

#endif // INST_UTIL_H_
