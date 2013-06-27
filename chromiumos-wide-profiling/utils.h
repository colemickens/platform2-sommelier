// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTILS_H_
#define UTILS_H_

#include <set>
#include <vector>

#include "base/basictypes.h"

#include "quipper_string.h"

namespace quipper {

extern const char* kSupportedMetadata[];

bool FileToBuffer(const string& filename, std::vector<char>* contents);

bool BufferToFile(const string& filename, const std::vector<char>& contents);

long int GetFileSize(const string& filename);

bool CompareFileContents(const string& file1, const string& file2);

uint64 Md5Prefix(const string& input);

bool CreateNamedTempFile(string* name);

// Returns true if the perf reports show the same summary.  Metadata
// is compared if it is present in kSupportedMetadata in utils.cc.
bool ComparePerfReports(const string& quipper_input,
                        const string& quipper_output);

// Similar to ComparePerfReports, but for piped perf data files.
// Warning: This is not commutative - |quipper_input| must be the piped perf
// data file passed to quipper, and |quipper_output| must be the file written
// by quipper.
bool ComparePipedPerfReports(const string& quipper_input,
                             const string& quipper_output,
                             std::set<string>* seen_metadata);

// Returns true if the perf buildid-lists are the same.
bool ComparePerfBuildIDLists(const string& file1, const string& file2);

// Adjust |size| to blocks of |align_size|.  i.e. returns the smallest multiple
// of |align_size| that can fit |size|.
uint64 AlignSize(uint64 size, uint32 align_size);

// Returns the size of the 8-byte-aligned memory for storing |string|.
size_t GetUint64AlignedStringLength(const string& str);

// Reads the contents of a file into |data|.  Returns true on success, false if
// it fails.
bool ReadFileToData(const string& filename, std::vector<char>* data);

// Writes contents of |data| to a file with name |filename|, overwriting any
// existing file.  Returns true on success, false if it fails.
bool WriteDataToFile(const std::vector<char>& data, const string& filename);

// Executes |command| and stores stdout output in |output|.  Returns true on
// success, false otherwise.
bool RunCommandAndGetStdout(const string& command, std::vector<char>* output);

}  // namespace quipper

#endif  // UTILS_H_
