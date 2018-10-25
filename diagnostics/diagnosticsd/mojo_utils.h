// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_MOJO_UTILS_H_
#define DIAGNOSTICS_DIAGNOSTICSD_MOJO_UTILS_H_

#include <cstdint>
#include <memory>

#include <base/memory/shared_memory.h>
#include <base/strings/string_piece.h>
#include <mojo/public/cpp/system/handle.h>

namespace diagnostics {

// Allows to get access to the buffer in read only shared memory. It converts
// mojo::Handle to base::SharedMemory.
//
// |handle| is the mojo handle of the buffer.
// |size| is the buffer size.
//
// Returns nullptr if error.
std::unique_ptr<base::SharedMemory> GetReadOnlySharedMemoryFromMojoHandle(
    mojo::ScopedHandle handle, int64_t size);

// Allocates buffer in shared memory, copies |content| to the buffer and
// converts shared buffer handle into |mojo::ScopedHandle|.
//
// Allocated shared memory is read only for another process.
//
// Returns invalid |mojo::ScopedHandle| if error happened.
mojo::ScopedHandle CreateReadOnlySharedMemoryMojoHandle(
    base::StringPiece content);

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_MOJO_UTILS_H_
