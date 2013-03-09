// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTILS_H_
#define UTILS_H_

#include <fstream>
#include <vector>

#include "base/basictypes.h"

#include "quipper_string.h"
#include "kernel/perf_internals.h"

bool FileToBuffer(const string& filename, std::vector<char>* contents);

bool BufferToFile(const string& filename,
                  const std::vector<char>& contents);

std::ifstream::pos_type GetFileSize(const string& filename);

bool CompareFileContents(const string& a, const string& b);

uint64 Md5Prefix(const string& input);

bool CreateNamedTempFile(string* name);

// Returns true if the perf reports show the same summary.  Metadata is not
// compared.
bool ComparePerfReports(const string& a, const string& b);

#endif  // UTILS_H_
