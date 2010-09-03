// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <login_manager/system_utils.h>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/scoped_temp_dir.h>
#include <gtest/gtest.h>

namespace login_manager {

TEST(SystemUtilsTest, CorrectFileWrite) {
  ScopedTempDir tmpdir;
  FilePath scratch;
  ASSERT_TRUE(tmpdir.CreateUniqueTempDir());
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir.path(), &scratch));

  std::string old_data("what");
  std::string new_data("ho, neighbor");

  ASSERT_EQ(old_data.length(),
            file_util::WriteFile(scratch, old_data.c_str(), old_data.length()));

  SystemUtils utils;
  ASSERT_TRUE(utils.AtomicFileWrite(scratch,
                                    new_data.c_str(),
                                    new_data.length()));
  std::string written_data;
  ASSERT_TRUE(file_util::ReadFileToString(scratch, &written_data));
  ASSERT_EQ(new_data, written_data);
}

}  // namespace login_manager
