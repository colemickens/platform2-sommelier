// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INIT_FILE_ATTRS_CLEANER_H_
#define INIT_FILE_ATTRS_CLEANER_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>

namespace file_attrs_cleaner {

extern const char xdg_origin_url[];
extern const char xdg_referrer_url[];

enum class AttributeCheckStatus {
  ERROR = 0,
  NO_ATTR,
  CLEAR_FAILED,
  CLEARED,
};

// Whether we allow |path| to be marked with immutable file attribute.
// If |path| is supposed to be a directory, set |isdir| to true.
bool ImmutableAllowed(const base::FilePath& path, bool isdir);

// Check the file attributes of the specified path.  |path| is used for logging
// and policy checking, so |fd| needs to be an open handle to it.  This helps
// with TOCTTOU issues.  If |path| is supposed to be a directory, set |isdir|
// to true.
AttributeCheckStatus CheckFileAttributes(const base::FilePath& path,
                                         bool isdir,
                                         int fd);

// Remove download-related URL extended attributes. See crbug.com/919486.
// This cannot use a file descriptor because the files we want to clear xattrs
// from are encrypted and therefore cannot be opened.
// Report whether the file actually had the relevant extended attributes for
// metrics purposes.
AttributeCheckStatus RemoveURLExtendedAttributes(const base::FilePath& path);

// Recursively scan the file attributes of paths under |dir|.
// Don't recurse into any subdirectories that exactly match any string in
// |skip_recurse|.
// Populate |num_url_xattrs| if files with those attributes were found.
bool ScanDir(const base::FilePath& dir,
             const std::vector<std::string>& skip_recurse,
             int32_t* url_xattrs_count);

// Convenience function.
static inline bool ScanDir(const std::string& dir,
                           const std::vector<std::string>& skip_recurse,
                           int* url_xattrs_count) {
  return ScanDir(base::FilePath(dir), skip_recurse, url_xattrs_count);
}

}  // namespace file_attrs_cleaner

#endif  // INIT_FILE_ATTRS_CLEANER_H_
