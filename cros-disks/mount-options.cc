// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mount-options.h"

#include <sys/mount.h>

#include <algorithm>

#include <base/string_util.h>

using std::pair;
using std::string;
using std::vector;

namespace cros_disks {

const char MountOptions::kOptionNoDev[] = "nodev";
const char MountOptions::kOptionNoExec[] = "noexec";
const char MountOptions::kOptionNoSuid[] = "nosuid";
const char MountOptions::kOptionReadOnly[] = "ro";
const char MountOptions::kOptionReadWrite[] = "rw";
const char MountOptions::kOptionSynchronous[] = "sync";

void MountOptions::Initialize(const vector<string>& options,
    bool set_user_and_group_id,
    const string& default_user_id, const string& default_group_id) {
  options_.clear();
  options_.reserve(options.size());

  bool option_read_only = false, option_read_write = false;
  string option_user_id, option_group_id;

  for (vector<string>::const_iterator
      option_iterator = options.begin(); option_iterator != options.end();
      ++option_iterator) {
    const string& option = *option_iterator;
    if (option == kOptionReadOnly) {
      option_read_only = true;
    } else if (option == kOptionReadWrite) {
      option_read_write = true;
    } else if (StartsWithASCII(option, "uid=", false)) {
      option_user_id = option;
    } else if (StartsWithASCII(option, "gid=", false)) {
      option_group_id = option;
    } else {
      options_.push_back(option);
    }
  }

  if (option_read_only || !option_read_write) {
    options_.push_back(kOptionReadOnly);
  } else {
    options_.push_back(kOptionReadWrite);
  }

  if (set_user_and_group_id) {
    if (!option_user_id.empty()) {
      options_.push_back(option_user_id);
    } else if (!default_user_id.empty()) {
      options_.push_back("uid=" + default_user_id);
    }

    if (!option_group_id.empty()) {
      options_.push_back(option_group_id);
    } else if (!default_group_id.empty()) {
      options_.push_back("gid=" + default_group_id);
    }
  }
}

bool MountOptions::IsReadOnlyOptionSet() const {
  for (vector<string>::const_reverse_iterator
      option_iterator = options_.rbegin(); option_iterator != options_.rend();
      ++option_iterator) {
    const string& option = *option_iterator;
    if (option == kOptionReadOnly)
      return true;

    if (option == kOptionReadWrite)
      return false;
  }
  return true;
}

void MountOptions::SetReadOnlyOption() {
  std::replace(options_.begin(), options_.end(),
      kOptionReadWrite, kOptionReadOnly);
}

pair<unsigned long, string>
MountOptions::ToMountFlagsAndData() const {
  unsigned long flags = MS_RDONLY;
  vector<string> data;
  data.reserve(options_.size());

  for (vector<string>::const_iterator
      option_iterator = options_.begin(); option_iterator != options_.end();
      ++option_iterator) {
    const string& option = *option_iterator;
    if (option == kOptionReadOnly) {
      flags |= MS_RDONLY;
    } else if (option == kOptionReadWrite) {
      flags &= ~static_cast<unsigned long>(MS_RDONLY);
    } else if (option == kOptionNoDev) {
      flags |= MS_NODEV;
    } else if (option == kOptionNoExec) {
      flags |= MS_NOEXEC;
    } else if (option == kOptionNoSuid) {
      flags |= MS_NOSUID;
    } else if (option == kOptionSynchronous) {
      flags |= MS_SYNCHRONOUS;
    } else {
      data.push_back(option);
    }
  }
  return std::make_pair(flags, JoinString(data, ','));
}

string MountOptions::ToString() const {
  return options_.empty() ? kOptionReadOnly : JoinString(options_, ',');
}

}  // namespace cros_disks
