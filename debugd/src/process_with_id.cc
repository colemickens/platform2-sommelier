// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "process_with_id.h"

#include <fcntl.h>
#include <unistd.h>

#include <base/string_number_conversions.h>

namespace debugd {

ProcessWithId::ProcessWithId() { }

bool ProcessWithId::Init() {
  return generate_id();
}

bool ProcessWithId::generate_id() {
  char buf[16];
  FILE *urandom = fopen("/dev/urandom", "r");
  if (!urandom) {
      PLOG(ERROR) << "Can't open /dev/urandom";
      return false;
  }
  if (fread(&buf, sizeof(buf), 1, urandom) != 1) {
      PLOG(ERROR) << "Can't read";
      fclose(urandom);
      return false;
  }

  id_ = base::HexEncode(buf, sizeof(buf));
  fclose(urandom);
  return true;
}

};  // namespace debugd
