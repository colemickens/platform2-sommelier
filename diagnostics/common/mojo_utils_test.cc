// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/common/mojo_utils.h"

#include <memory>
#include <utility>

#include <base/memory/shared_memory.h>
#include <base/strings/string_piece.h>
#include <gtest/gtest.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/public/cpp/system/handle.h>

namespace diagnostics {

namespace {

class MojoUtilsTest : public testing::Test {
 protected:
  MojoUtilsTest() { mojo::edk::Init(); }
};

}  // namespace

// TODO(crbug.com/946330): Disabled due to flakiness.
TEST_F(MojoUtilsTest, DISABLED_CreateMojoHandleAndRetrieveContent) {
  const base::StringPiece content("{\"key\": \"value\"}");

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
