// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromiumos-wide-profiling/dso.h"

#include <elf.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

#include "base/logging.h"
#include "chromiumos-wide-profiling/compat/string.h"
#include "chromiumos-wide-profiling/compat/test.h"
#include "chromiumos-wide-profiling/dso_test_utils.h"
#include "chromiumos-wide-profiling/scoped_temp_path.h"
#include "chromiumos-wide-profiling/utils.h"

namespace quipper {

TEST(DsoTest, ReadsBuildId) {
  InitializeLibelf();
  ScopedTempFile elf("/tmp/tempelf.");

  const string expected_buildid = "\xde\xad\xf0\x0d";
  testing::WriteElfWithBuildid(elf.path(), ".note.gnu.build-id",
                               expected_buildid);

  string buildid;
  EXPECT_TRUE(ReadElfBuildId(elf.path(), &buildid));
  EXPECT_EQ(expected_buildid, buildid);

  // Repeat with other spellings of the section name:

  testing::WriteElfWithBuildid(elf.path(), ".notes", expected_buildid);
  EXPECT_TRUE(ReadElfBuildId(elf.path(), &buildid));
  EXPECT_EQ(expected_buildid, buildid);

  testing::WriteElfWithBuildid(elf.path(), ".note", expected_buildid);
  EXPECT_TRUE(ReadElfBuildId(elf.path(), &buildid));
  EXPECT_EQ(expected_buildid, buildid);
}

TEST(DsoTest, ReadsBuildId_MissingBuildid) {
  InitializeLibelf();
  ScopedTempFile elf("/tmp/tempelf.");

  testing::WriteElfWithMultipleBuildids(elf.path(), {/*empty*/});

  string buildid;
  EXPECT_FALSE(ReadElfBuildId(elf.path(), &buildid));
}

TEST(DsoTest, ReadsBuildId_WrongSection) {
  InitializeLibelf();
  ScopedTempFile elf("/tmp/tempelf.");

  testing::WriteElfWithBuildid(elf.path(), ".unexpected-section", "blah");

  string buildid;
  EXPECT_FALSE(ReadElfBuildId(elf.path(), &buildid));
}

TEST(DsoTest, ReadsBuildId_PrefersGnuBuildid) {
  InitializeLibelf();
  ScopedTempFile elf("/tmp/tempelf.");

  const string buildid_gnu = "\xde\xad\xf0\x0d";
  const string buildid_notes = "\xc0\xde\xf0\x0d";
  const string buildid_note = "\xfe\xed\xba\xad";

  std::vector<std::pair<string, string>> section_buildids {
    std::make_pair(".notes", buildid_notes),
    std::make_pair(".note", buildid_note),
    std::make_pair(".note.gnu.build-id", buildid_gnu),
  };
  testing::WriteElfWithMultipleBuildids(elf.path(), section_buildids);

  string buildid;
  EXPECT_TRUE(ReadElfBuildId(elf.path(), &buildid));
  EXPECT_EQ(buildid_gnu, buildid);

  // Also prefer ".notes" over ".note"
  section_buildids = {
    std::make_pair(".note", buildid_note),
    std::make_pair(".notes", buildid_notes),
  };
  testing::WriteElfWithMultipleBuildids(elf.path(), section_buildids);

  EXPECT_TRUE(ReadElfBuildId(elf.path(), &buildid));
  EXPECT_EQ(buildid_notes, buildid);
}

}  // namespace quipper
