// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/quote.h"

#include <iomanip>

namespace cros_disks {

std::ostream& operator<<(std::ostream& out, Quoter<const char*> quoter) {
  const char* const s = quoter.ref;
  return s ? out << std::quoted(s, '\'') : out << "(null)";
}

std::ostream& operator<<(std::ostream& out, Quoter<std::string> quoter) {
  return out << std::quoted(quoter.ref, '\'');
}

std::ostream& operator<<(std::ostream& out, Quoter<base::FilePath> quoter) {
  return out << std::quoted(quoter.ref.value(), '\'');
}

}  // namespace cros_disks
