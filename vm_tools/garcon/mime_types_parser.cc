// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_split.h>
#include <base/strings/string_tokenizer.h>
#include <base/strings/string_util.h>

#include "vm_tools/garcon/mime_types_parser.h"

namespace {
// Ridiculously large size for a mime.types file.
constexpr size_t kMaxMimeTypesFileSize = 10485760;  // 10 MB
}  // namespace

namespace vm_tools {
namespace garcon {

bool ParseMimeTypes(const std::string& file_name, MimeTypeMap* out_mime_types) {
  CHECK(out_mime_types);
  base::FilePath file_path(file_name);
  if (!base::PathExists(file_path)) {
    LOG(ERROR) << "MIME types file does not exist at: " << file_name;
    return false;
  }

  std::string file_contents;
  if (!base::ReadFileToStringWithMaxSize(file_path, &file_contents,
                                         kMaxMimeTypesFileSize)) {
    LOG(ERROR) << "Failed reading in mime.types file: " << file_name;
    return false;
  }

  std::vector<base::StringPiece> lines = base::SplitStringPiece(
      file_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& curr_line : lines) {
    // Each line will have 1 or more tokens separated by whitespace. The first
    // token will be a MIME type. If additional tokens are present, they are the
    // file extensions for that MIME type. Commented lines begin with #.
    if (curr_line[0] == '#')
      continue;

    std::vector<base::StringPiece> tokens = base::SplitStringPiece(
        curr_line, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    if (tokens.size() < 2)
      continue;
    std::string base_token = tokens[0].as_string();
    for (int i = 1; i < tokens.size(); ++i) {
      (*out_mime_types)[tokens[i].as_string()] = base_token;
    }
  }

  return true;
}

}  // namespace garcon
}  // namespace vm_tools
