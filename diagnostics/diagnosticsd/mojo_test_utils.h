// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_MOJO_TEST_UTILS_H_
#define DIAGNOSTICS_DIAGNOSTICSD_MOJO_TEST_UTILS_H_

#include <memory>
#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>

namespace diagnostics {

// Helper class that allows to obtain fake file descriptors for use in tests
// where a valid file descriptor is expected.
class FakeMojoFdGenerator final {
 public:
  FakeMojoFdGenerator();
  ~FakeMojoFdGenerator();

  // Returns a duplicate of the file descriptor held by this instance.
  base::ScopedFD MakeFd() const;

  // Returns whether the given file descriptor points to the same underlying
  // object as the file descriptor held by this instance.
  bool IsDuplicateFd(int another_fd) const;

 private:
  base::ScopedFD fd_;

  DISALLOW_COPY_AND_ASSIGN(FakeMojoFdGenerator);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_MOJO_TEST_UTILS_H_
