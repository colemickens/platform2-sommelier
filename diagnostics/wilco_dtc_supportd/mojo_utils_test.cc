// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/mojo_utils.h"

#include <utility>

#include <base/strings/string_piece.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mojo/public/cpp/system/handle.h>

namespace diagnostics {

TEST(MojoUtils, CreateMojoHandleAndRetrieveContent) {
  base::StringPiece content("{\"key\": \"value\"}");

  mojo::ScopedHandle handle = CreateReadOnlySharedMemoryMojoHandle(content);
  EXPECT_TRUE(handle.is_valid());

  std::unique_ptr<base::SharedMemory> shared_memory =
      GetReadOnlySharedMemoryFromMojoHandle(std::move(handle));
  ASSERT_TRUE(shared_memory);

  base::StringPiece actual(static_cast<char*>(shared_memory->memory()),
                           shared_memory->mapped_size());
  EXPECT_EQ(content, actual);
}

}  // namespace diagnostics
