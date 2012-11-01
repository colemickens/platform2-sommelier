// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTILS_H_
#define UTILS_H_

#include <fstream>
#include <vector>

#include "common.h"


bool FileToBuffer(const std::string & filename, std::vector<char> * contents);
bool BufferToFile(const std::string & filename,
                  const std::vector<char> & contents);
std::ifstream::pos_type GetFileSize(const std::string & filename);
bool CompareFileContents(const std::string & a, const std::string & b);
uint64 Md5Prefix(const std::string input);


#endif /*UTILS_H_*/
