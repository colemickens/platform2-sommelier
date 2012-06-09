// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "chromeos_install_config.h"
#include "chromeos_postinst.h"
#include "inst_util.h"

using std::string;

class UtilTest : public ::testing::Test { };

TEST(UtilTest, StringPrintfTest) {
  EXPECT_EQ(StringPrintf(""), "");
  EXPECT_EQ(StringPrintf("Stuff"), "Stuff");
  EXPECT_EQ(StringPrintf("%s", "Stuff"), "Stuff");
  EXPECT_EQ(StringPrintf("%d", 3), "3");
  EXPECT_EQ(StringPrintf("%s %d", "Stuff", 3), "Stuff 3");
}

TEST(UtilTest, SplitStringTest) {
  std::vector<std::string> result;

  SplitString("My Stuff", ',', &result);
  ASSERT_THAT(result, testing::ElementsAre("My Stuff"));

  SplitString("My,Stuff,Is", ',', &result);
  ASSERT_THAT(result, testing::ElementsAre("My", "Stuff", "Is"));

  SplitString(",My,Stuff", ',', &result);
  ASSERT_THAT(result, testing::ElementsAre("", "My", "Stuff"));

  SplitString("My,Stuff,", ',', &result);
  ASSERT_THAT(result, testing::ElementsAre("My", "Stuff", ""));
}

TEST(UtilTest, RunCommandTest) {
  // Note that RunCommand returns the raw system() result, including signal
  // values. WEXITSTATUS would be needed to check clean result codes.
  EXPECT_EQ(RunCommand("/bin/true"), 0);
  EXPECT_EQ(RunCommand("/bin/false"), 256);
  EXPECT_EQ(RunCommand("/bin/bogus"), 32512);
  EXPECT_EQ(RunCommand("/bin/bash -c \"exit 2\""), 512);
  EXPECT_EQ(RunCommand("/bin/echo RunCommand*Test"), 0);
}

TEST(UtilTest, ReadFileToStringTest) {
  string result;

  // This constant should match the contents of lsb-release-test.txt exactly.
  const char* LSB_CONTENTS =
      "CHROMEOS_RELEASE_BOARD=x86-mario\n"
      "CHROMEOS_RELEASE=1568.0.2012_01_19_1424\n"
      "CHROMEOS_AUSERVER=http://blah.blah:8080/update\n";

  // Non-existent file
  EXPECT_EQ(ReadFileToString("/foo", &result), false);

  // A directory, not file
  EXPECT_EQ(ReadFileToString("/tmp", &result), false);

  // A file with known contents
  EXPECT_EQ(ReadFileToString("lsb-release-test.txt", &result), true);
  EXPECT_EQ(result, LSB_CONTENTS);

  // A larger file, but without known contents
  EXPECT_EQ(ReadFileToString("LICENSE", &result), true);
}

TEST(UtilTest, WriteStringToFileTest) {
  const string file = "/tmp/fuzzy";
  string read_contents;

  // rm it, if it exists, ignore error if it doesn't
  unlink(file.c_str());

  // Attempt to write to a directory, not file
  EXPECT_EQ(WriteStringToFile("fuzzy", "/tmp"), false);

  // Attempt to create file in non-existant directory
  EXPECT_EQ(WriteStringToFile("fuzzy", "/fuzzy/wuzzy"), false);

  // Attempt to create file in /tmp
  EXPECT_EQ(WriteStringToFile("fuzzy", file), true);
  EXPECT_EQ(ReadFileToString(file, &read_contents), true);
  EXPECT_EQ("fuzzy", read_contents);

  // Attempt to overwrite a file in /tmp
  EXPECT_EQ(WriteStringToFile("foobar", file), true);
  EXPECT_EQ(ReadFileToString(file, &read_contents), true);
  EXPECT_EQ("foobar", read_contents);

  // Cleanup
  unlink(file.c_str());
}

TEST(UtilTest, CopyFileTest) {
  const string file1 = "/tmp/fuzzy";
  const string file2 = "/tmp/wuzzy";
  const string contents = "file contents";

  string license_contents;
  string read_contents;

  WriteStringToFile("file contents", file1);
  unlink(file2.c_str());

  // Copy directory to file
  EXPECT_EQ(CopyFile("/tmp", file2), false);

  // Copy nonexistent file
  EXPECT_EQ(CopyFile("/tmp/nonexistent_file", file2), false);

  // Copy existent file to diretory
  EXPECT_EQ(CopyFile(file1, "/tmp"), false);

  // Copy existent to non-existent
  EXPECT_EQ(CopyFile(file1, file2), true);
  EXPECT_EQ(ReadFileToString(file2, &read_contents), true);
  EXPECT_EQ(contents, read_contents);

  // Copy existent to existent
  WriteStringToFile("different file contents", file2);
  EXPECT_EQ(CopyFile(file1, file2), true);
  EXPECT_EQ(ReadFileToString(file2, &read_contents), true);
  EXPECT_EQ(contents, read_contents);

  // Copy larger file to existent
  EXPECT_EQ(CopyFile("LICENSE", file2), true);
  EXPECT_EQ(ReadFileToString("LICENSE", &license_contents), true);
  EXPECT_EQ(ReadFileToString(file2, &read_contents), true);
  EXPECT_EQ(license_contents, read_contents);

  unlink(file1.c_str());
  unlink(file2.c_str());
}

TEST(UtilTest, LsbReleaseValueTest) {
  string result_string;

  EXPECT_EQ(LsbReleaseValue("bogus",
                            "CHROMEOS_RELEASE_BOARD",
                            &result_string),
            false);

  EXPECT_EQ(LsbReleaseValue("lsb-release-test.txt",
                            "CHROMEOS_RELEASE_BOARD",
                            &result_string),
            true);
  EXPECT_EQ(result_string, "x86-mario");

  EXPECT_EQ(LsbReleaseValue("lsb-release-test.txt",
                            "CHROMEOS_RELEASE",
                            &result_string),
            true);
  EXPECT_EQ(result_string, "1568.0.2012_01_19_1424");

  EXPECT_EQ(LsbReleaseValue("lsb-release-test.txt",
                            "CHROMEOS_AUSERVER",
                            &result_string),
            true);
  EXPECT_EQ(result_string, "http://blah.blah:8080/update");
};

TEST(UtilTest, VersionLessTest) {
  EXPECT_EQ(VersionLess("12.13.2.4", "12.13.2.4"), false); // 4 digit ==
  EXPECT_EQ(VersionLess("12.13.2.3", "12.13.2.4"), true);  // 4 digit <
  EXPECT_EQ(VersionLess("12.13.2.4", "12.13.2.3"), false); // 4 digit >
  EXPECT_EQ(VersionLess("12.13.2", "12.13.2"), false);     // 3 digit ==
  EXPECT_EQ(VersionLess("12.13.1", "12.13.2"), true);      // 3 digit <
  EXPECT_EQ(VersionLess("12.13.4", "12.13.3"), false);     // 3 digit >
  EXPECT_EQ(VersionLess("12.13.2", "12.14.1"), true);      // 3 digit >
  EXPECT_EQ(VersionLess("12.13.2", "1.13.2.4"), false);    // 3 digit, 4 digit
  EXPECT_EQ(VersionLess("12.13.2.4", "12.13.1"), true);    // 4 digit, 3 digit
};

TEST(UtilTest, GetBlockDevFromPartitionDev) {
  EXPECT_EQ(GetBlockDevFromPartitionDev("/dev/sda3"), "/dev/sda");
  EXPECT_EQ(GetBlockDevFromPartitionDev("/dev/sda321"), "/dev/sda");
  EXPECT_EQ(GetBlockDevFromPartitionDev("/dev/sda"), "/dev/sda");
  EXPECT_EQ(GetBlockDevFromPartitionDev("/dev/mmcblk0p3"), "/dev/mmcblk0");
  EXPECT_EQ(GetBlockDevFromPartitionDev("/dev/mmcblk12p321"), "/dev/mmcblk12");
  EXPECT_EQ(GetBlockDevFromPartitionDev("/dev/mmcblk0"), "/dev/mmcblk0");
  EXPECT_EQ(GetBlockDevFromPartitionDev(""), "");
}

TEST(UtilTest, GetPartitionDevTest) {
  EXPECT_EQ(GetPartitionFromPartitionDev("/dev/sda3"), 3);
  EXPECT_EQ(GetPartitionFromPartitionDev("/dev/sda321"), 321);
  EXPECT_EQ(GetPartitionFromPartitionDev("/dev/sda"), 0);
  EXPECT_EQ(GetPartitionFromPartitionDev("/dev/mmcblk0p3"), 3);
  EXPECT_EQ(GetPartitionFromPartitionDev("/dev/mmcblk12p321"), 321);
  EXPECT_EQ(GetPartitionFromPartitionDev("/dev/mmcblk1"), 0);
  EXPECT_EQ(GetPartitionFromPartitionDev("3"), 3);
  EXPECT_EQ(GetPartitionFromPartitionDev(""), 0);
}

TEST(UtilTest, MakePartitionDevTest) {
  EXPECT_EQ(MakePartitionDev("/dev/sda", 3), "/dev/sda3");
  EXPECT_EQ(MakePartitionDev("/dev/sda", 321), "/dev/sda321");
  EXPECT_EQ(MakePartitionDev("/dev/mmcblk0", 3), "/dev/mmcblk0p3");
  EXPECT_EQ(MakePartitionDev("/dev/mmcblk12", 321), "/dev/mmcblk12p321");
  EXPECT_EQ(MakePartitionDev("", 0), "0");
}

TEST(UtilTest, DirnameTest) {
  EXPECT_EQ(Dirname("/mnt/dir/postinst"), "/mnt/dir");
  EXPECT_EQ(Dirname("/mnt/dir/"), "/mnt/dir");
  EXPECT_EQ(Dirname("file"), "");
  EXPECT_EQ(Dirname("/"), "");
  EXPECT_EQ(Dirname(""), "");
}

TEST(UtilTest, RemovePackFileTest) {
  // Setup
  EXPECT_EQ(RunCommand("rm -rf /tmp/PackFileTest"), 0);
  EXPECT_EQ(RunCommand("mkdir /tmp/PackFileTest"), 0);

  EXPECT_EQ(Touch("/tmp/PackFileTest/foo"), true);
  EXPECT_EQ(Touch("/tmp/PackFileTest/foo.pack"), true);
  EXPECT_EQ(Touch("/tmp/PackFileTest/foopack"), true);
  EXPECT_EQ(Touch("/tmp/PackFileTest/.foo.pack"), true);

  // Test
  EXPECT_EQ(RemovePackFiles("/tmp/PackFileTest"), true);

  // Test to see which files were removed
  struct stat stats;

  EXPECT_EQ(stat("/tmp/PackFileTest/foo", &stats), 0);
  EXPECT_EQ(stat("/tmp/PackFileTest/foo.pack", &stats), -1);
  EXPECT_EQ(stat("/tmp/PackFileTest/foopack", &stats), -1);
  EXPECT_EQ(stat("/tmp/PackFileTest/.foo.pack", &stats), 0);

  // Bad dir name
  EXPECT_EQ(RemovePackFiles("/fuzzy"), false);

  // Cleanup
  EXPECT_EQ(RunCommand("rm -rf /tmp/PackFileTest"), 0);
}

TEST(UtilTest, TouchTest) {
  unlink("/tmp/fuzzy");

  // Touch a non-existent file
  EXPECT_EQ(Touch("/tmp/fuzzy"), true);

  // Touch an existent file
  EXPECT_EQ(Touch("/tmp/fuzzy"), true);

  // This touch creates files, and so can't touch a dir
  EXPECT_EQ(Touch("/tmp"), false);

  // Bad Touch
  EXPECT_EQ(Touch("/fuzzy/wuzzy"), false);

  unlink("/tmp/fuzzy");
}

TEST(UtilTest, ReplaceInFileTest) {
  const string file = "/tmp/fuzzy";
  const string start = "Fuzzy Wuzzy was a lamb";
  string finish;

  // File doesn't exist
  EXPECT_EQ(ReplaceInFile("was", "wuz", "/fuzzy/wuzzy"), false);

  // Change middle, same length
  EXPECT_EQ(WriteStringToFile(start, file), true);
  EXPECT_EQ(ReplaceInFile("was", "wuz", file), true);
  EXPECT_EQ(ReadFileToString(file, &finish), true);
  EXPECT_EQ(finish, "Fuzzy Wuzzy wuz a lamb");

  // Change middle, longer
  EXPECT_EQ(WriteStringToFile(start, file), true);
  EXPECT_EQ(ReplaceInFile("was", "wasn't", file), true);
  EXPECT_EQ(ReadFileToString(file, &finish), true);
  EXPECT_EQ(finish, "Fuzzy Wuzzy wasn't a lamb");

  // Change middle, shorter
  EXPECT_EQ(WriteStringToFile(start, file), true);
  EXPECT_EQ(ReplaceInFile("Wuzzy", "Wuz", file), true);
  EXPECT_EQ(ReadFileToString(file, &finish), true);
  EXPECT_EQ(finish, "Fuzzy Wuz was a lamb");

  // Change beginning, longer
  EXPECT_EQ(WriteStringToFile(start, file), true);
  EXPECT_EQ(ReplaceInFile("Fuzzy", "AFuzzy", file), true);
  EXPECT_EQ(ReadFileToString(file, &finish), true);
  EXPECT_EQ(finish, "AFuzzy Wuzzy was a lamb");

  // Change end, shorter
  EXPECT_EQ(WriteStringToFile(start, file), true);
  EXPECT_EQ(ReplaceInFile("lamb", "la", file), true);
  EXPECT_EQ(ReadFileToString(file, &finish), true);
  EXPECT_EQ(finish, "Fuzzy Wuzzy was a la");
}

TEST(UtilTest, R10FileSystemPatchTest) {
  const string file = "/tmp/fuzzy";

  EXPECT_EQ(Touch(file), true);

  // Should modify file
  EXPECT_EQ(R10FileSystemPatch(file), true);

  // Check that the file has had bytes 1401 and 1402 written too.
  struct stat stats;
  EXPECT_EQ(stat(file.c_str(), &stats), 0);
  EXPECT_EQ(stats.st_size, 1402);

  unlink(file.c_str());
};

TEST(UtilTest, ExtractKernelArgTest) {
  string kernel_config =
      "root=/dev/dm-1 dm=\"foo bar, ver=2 root2=1 stuff=v\""
      " fuzzy=wuzzy root2=/dev/dm-2";
  string dm_config = "foo bar, ver=2 root2=1 stuff=v";

  // kernel config
  EXPECT_EQ(ExtractKernelArg(kernel_config, "root"), "/dev/dm-1");
  EXPECT_EQ(ExtractKernelArg(kernel_config, "root2"), "/dev/dm-2");
  EXPECT_EQ(ExtractKernelArg(kernel_config, "dm"), dm_config);

  // Corrupt config
  EXPECT_EQ(ExtractKernelArg("root=\"", "root"), "");
  EXPECT_EQ(ExtractKernelArg("root=\" bar", "root"), "");

  // Inside dm config
  EXPECT_EQ(ExtractKernelArg(dm_config, "ver"), "2");
  EXPECT_EQ(ExtractKernelArg(dm_config, "stuff"), "v");
  EXPECT_EQ(ExtractKernelArg(dm_config, "root"), "");
}

TEST(UtilTest, SetKernelArgTest) {
  const string kernel_config =
      "root=/dev/dm-1 dm=\"foo bar, ver=2 root2=1 stuff=v\""
      " fuzzy=wuzzy root2=/dev/dm-2";

  string working_config;

  // Basic change
  working_config = kernel_config;
  EXPECT_EQ(SetKernelArg("fuzzy", "tuzzy", &working_config), true);
  EXPECT_EQ(working_config,
            "root=/dev/dm-1 dm=\"foo bar, ver=2 root2=1 stuff=v\""
            " fuzzy=tuzzy root2=/dev/dm-2");

  // Empty a value
  working_config = kernel_config;
  EXPECT_EQ(SetKernelArg("root", "", &working_config), true);
  EXPECT_EQ(working_config,
            "root= dm=\"foo bar, ver=2 root2=1 stuff=v\""
            " fuzzy=wuzzy root2=/dev/dm-2");

  // Set a value that requires quotes
  working_config = kernel_config;
  EXPECT_EQ(SetKernelArg("root", "a b", &working_config), true);
  EXPECT_EQ(working_config,
            "root=\"a b\" dm=\"foo bar, ver=2 root2=1 stuff=v\""
            " fuzzy=wuzzy root2=/dev/dm-2");

  // Change a value that requires quotes to be removed
  working_config = kernel_config;
  EXPECT_EQ(SetKernelArg("dm", "ab", &working_config), true);
  EXPECT_EQ(working_config,
            "root=/dev/dm-1 dm=ab fuzzy=wuzzy root2=/dev/dm-2");

  // Change a quoted value that stays quoted
  working_config = kernel_config;
  EXPECT_EQ(SetKernelArg("dm", "a b", &working_config), true);
  EXPECT_EQ(working_config,
            "root=/dev/dm-1 dm=\"a b\" fuzzy=wuzzy root2=/dev/dm-2");

  // Try to change value that's not present
  working_config = kernel_config;
  EXPECT_EQ(SetKernelArg("unknown", "", &working_config), false);
  EXPECT_EQ(working_config, kernel_config);

  // Try to change a term inside quotes to ensure it's ignored
  working_config = kernel_config;
  EXPECT_EQ(SetKernelArg("ver", "", &working_config), false);
  EXPECT_EQ(working_config, kernel_config);
}
