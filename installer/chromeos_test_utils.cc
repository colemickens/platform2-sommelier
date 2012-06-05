// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_test_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "chromeos_install_config.h"
#include "chromeos_postinst.h"
#include "inst_util.h"

using std::string;

// This constant should match the contents of lsb-release-test.txt exactly.
const char* LSB_CONTENTS = "CHROMEOS_RELEASE_BOARD=x86-mario\n"
                           "CHROMEOS_RELEASE=1568.0.2012_01_19_1424\n"
                           "CHROMEOS_AUSERVER=http://blah.blah:8080/update\n";

void TestStringPrintf(const string& result,
                      const string& expected) {
  if (result != expected) {
    printf("StringPrintf('%s', '%s') Didn't match\n",
           result.c_str(), expected.c_str());
    exit(1);
  }
}

void TestSplitString(const string& str,
                     char split,
                     const std::vector<std::string>& expected) {
  std::vector<std::string> result;
  SplitString(str, split, &result);

  if (result != expected) {
    printf("SplitString('%s', '%c') Didn't match\n",
           str.c_str(), split);
    exit(1);
  }
}

void TestRunCommand(const string &cmd, int expected) {
  int result = RunCommand(cmd);
  if (result != expected) {
    printf("Runcommand(%s) %d != %d\n",
           cmd.c_str(), result, expected);
    exit(1);
  }
}

void TestVersion(const string &left, const string &right, bool expected) {
  if (VersionLess(left, right) != expected) {
    printf("VersionLess(%s, %s) != %d\n",
           left.c_str(), right.c_str(), expected);
    exit(1);
  }
}

// expected_string is optional, pass NULL if you don't want to verify
// the string passed in.
void TestReadFileToString(const string &file,
                          bool expected_success,
                          const char* expected_string) {
  string result_string;
  bool result_success = ReadFileToString(file, &result_string);

  if (result_success != expected_success) {
    printf("ReadFileToString(%s) '%s' != %s\n",
           file.c_str(),
           result_success ? "success" : "failure",
           expected_success ? "success" : "failure");
    exit(1);
  }

  if (!expected_string)
    return;

  if (result_string != expected_string) {
    printf("ReadFileToString(%s) '%s' != %s\n",
           file.c_str(),
           result_string.c_str(), expected_string);
    exit(1);
  }
}

void TestWriteStringToFile(const string &contents,
                           const string &file,
                           bool expected_success) {
  bool result_success = WriteStringToFile(contents, file);

  if (result_success != expected_success) {
    printf("WriteStringToFile(%s, %s) '%s' != %s\n",
           contents.c_str(),
           file.c_str(),
           result_success ? "success" : "failure",
           expected_success ? "success" : "failure");
    exit(1);
  }

  if (!expected_success)
    return;

  string result_string;
  result_success = ReadFileToString(file, &result_string);

  if (!result_success) {
    printf("TestWriteStringToFile failed to read %s\n", file.c_str());
    exit(1);
  }

  if (result_string != contents) {
    printf("TestWriteStringToFile contents mismatch '%s' != '%s'\n",
           result_string.c_str(),
           contents.c_str());
    exit(1);
  }
}

void TestCopyFile(const string &file_from,
                  const string &file_to,
                  bool expected_success) {
  bool result_success = CopyFile(file_from, file_to);

  if (result_success != expected_success) {
    printf("CopyFile(%s, %s) '%s' != %s\n",
           file_from.c_str(),
           file_to.c_str(),
           result_success ? "success" : "failure",
           expected_success ? "success" : "failure");
    exit(1);
  }

  if (!expected_success)
    return;

  string from_contents;
  string to_contents;

  if (!ReadFileToString(file_from, &from_contents) ||
      !ReadFileToString(file_to, &to_contents)) {
    printf("Failed to read files: %s, %s", file_from.c_str(), file_to.c_str());
    exit(1);
  }

  if (from_contents != to_contents) {
    printf("Copy contents don't match: '%s', '%s'",
           from_contents.c_str(), to_contents.c_str());
    exit(1);
  }
}


void TestLsb(const string &file,
             const string &key,
             bool expected_success,
             const char* expected_string) {
  string result_string;
  bool result_success = LsbReleaseValue(file, key, &result_string);

  if (result_success != expected_success) {
    printf("LsbReleaseValue(%s) '%s' != %s\n",
           file.c_str(),
           result_success ? "success" : "failure",
           expected_success ? "success" : "failure");
    exit(1);
  }

  if (!expected_string)
    return;

  if (result_string != expected_string) {
    printf("LsbReleaseValue(%s) '%s' != %s\n",
           file.c_str(),
           result_string.c_str(), expected_string);
    exit(1);
  }
}

void TestGetBlockDevFromPartitionDev(const string &partition_dev,
                                     const string &expected) {
  string result = GetBlockDevFromPartitionDev(partition_dev);
  if (result != expected) {
    printf("GetBlockDevFromPartitionDev(%s) '%s' != %s\n",
           partition_dev.c_str(),
           result.c_str(), expected.c_str());
    exit(1);
  }
}

void TestGetPartitionFromPartitionDev(const string &partition_dev,
                                      int expected) {
  int result = GetPartitionFromPartitionDev(partition_dev);
  if (result != expected) {
    printf("GetPartitionFromPartitionDev(%s) '%d' != %d\n",
           partition_dev.c_str(),
           result, expected);
    exit(1);
  }
}

void TestMakePartitionDev(const string &block_dev,
                          int partition,
                          const string &expected) {
  string result = MakePartitionDev(block_dev, partition);
  if (result != expected) {
    printf("GetMakePartitionDev(%s, %d) '%s' != %s\n",
           block_dev.c_str(), partition,
           result.c_str(), expected.c_str());
    exit(1);
  }
}

void TestDirname(const string &filename,
                 const string &expected) {
  string result = Dirname(filename);
  if (result != expected) {
    printf("Dirname(%s) '%s' != %s\n",
           filename.c_str(),
           result.c_str(), expected.c_str());
    exit(1);
  }
}

void TestTouch(const string &filename) {
  bool result = Touch(filename);
  if (!result) {
    printf("Touch('%s') Failed\n", filename.c_str());
    exit(1);
  }
}

void TestReplaceInFile(const string& pattern,
                       const string& value,
                       const string& path,
                       const string& source_contents,
                       bool expected_success,
                       const string& expected_string) {
  // If the write fails, we ignore it and test to see if the replace fails
  // when it can't open the file in question.
  WriteStringToFile(source_contents, path);

  if (ReplaceInFile(pattern, value, path) != expected_success) {
    printf("ReplaceInFile result wasn't %s as expected\n",
           expected_success ? "Success" : "Failure");
    exit(1);
  }

  if (!expected_success) {
    return;
  }

  string result_string;
  if (!ReadFileToString(path, &result_string)) {
    exit(1);
  }

  if (result_string != expected_string) {
    printf("ReplaceInFile result '%s' was not: '%s'\n",
           result_string.c_str(),
           expected_string.c_str());
    exit(1);
  }
}

void TestRemovePackFiles(const string &dirname) {
  bool result = RemovePackFiles(dirname);
  if (!result) {
    printf("RemovePackFiles('%s') Failed\n", dirname.c_str());
    exit(1);
  }
}

void TestR10FileSystemPatch(const string &dev_name) {
  bool result = R10FileSystemPatch(dev_name);
  if (!result) {
    printf("R10FileSystemPatch('%s') Failed\n", dev_name.c_str());
    exit(1);
  }
}

void TestMakeDeviceReadOnly(const string &dev_name) {
  bool result = MakeDeviceReadOnly(dev_name);
  if (!result) {
    printf("MakeDeviceReadOnly('%s') Failed\n", dev_name.c_str());
    exit(1);
  }
}

void TestExtractKernelArg(const string &kernel_config,
                          const string &tag,
                          const string &expected) {
  string result = ExtractKernelArg(kernel_config, tag);
  if (result != expected) {
    printf("ExtractKernelArg(%s, %s) '%s' != '%s'\n",
           kernel_config.c_str(), tag.c_str(),
           result.c_str(), expected.c_str());
    exit(1);
  }
}

void TestSetKernelArg(const string &kernel_config,
                      const string &tag,
                      const string &value,
                      bool expected_success,
                      const string &expected_result) {

  string working_config = kernel_config;

  bool result_success = SetKernelArg(tag, value, &working_config);

  if (result_success != expected_success) {
    printf("SetKernelArg(%s, %s, %s) '%s' != %s\n",
           tag.c_str(),
           value.c_str(),
           kernel_config.c_str(),
           result_success ? "success" : "failure",
           expected_success ? "success" : "failure");
    exit(1);
  }

  if (expected_success) {
    if (working_config != expected_result) {
      printf("SetKernelArg(%s, %s, %s) \n'%s' != \n'%s'\n",
             tag.c_str(),
             value.c_str(),
             kernel_config.c_str(),
             working_config.c_str(),
             expected_result.c_str());
      exit(1);
    }
  } else {
    // If the method failed, working_config should be unmodfied.
    if (working_config != kernel_config) {
      printf("SetKernelArg(%s, %s, %s) '%s' != %s\n",
             tag.c_str(),
             value.c_str(),
             kernel_config.c_str(),
             working_config.c_str(),
             kernel_config.c_str());
      exit(1);
    }
  }
}

void TestConfigureInstall(const std::string& install_dev,
                          const std::string& install_path,
                          bool expected_success,
                          const std::string& expected_slot,
                          const std::string& expected_root,
                          const std::string& expected_kernel,
                          const std::string& expected_boot) {

  InstallConfig install_config;

  bool result_success = ConfigureInstall(install_dev,
                                         install_path,
                                         &install_config);

  if (result_success != expected_success) {
    printf("TestConfigureInstall(%s, %s) '%s' != %s\n",
           install_dev.c_str(),
           install_path.c_str(),
           result_success ? "success" : "failure",
           expected_success ? "success" : "failure");
    exit(1);
  }

  if (!expected_success)
    return;

  if (install_config.slot != expected_slot) {
    printf("TestConfigureInstall(%s, %s) slot '%s' != %s\n",
           install_dev.c_str(),
           install_path.c_str(),
           install_config.slot.c_str(),
           expected_slot.c_str());
    exit(1);
  }

  if (install_config.root.device() != expected_root) {
    printf("TestConfigureInstall(%s, %s) root '%s' != %s\n",
           install_dev.c_str(),
           install_path.c_str(),
           install_config.root.device().c_str(),
           expected_root.c_str());
    exit(1);
  }

  if (install_config.kernel.device() != expected_kernel) {
    printf("TestConfigureInstall(%s, %s) kernel '%s' != %s\n",
           install_dev.c_str(),
           install_path.c_str(),
           install_config.kernel.device().c_str(),
           expected_kernel.c_str());
    exit(1);
  }

  if (install_config.boot.device() != expected_boot) {
    printf("TestConfigureInstall(%s, %s) kernel '%s' != %s\n",
           install_dev.c_str(),
           install_path.c_str(),
           install_config.boot.device().c_str(),
           expected_boot.c_str());
    exit(1);
  }
}


void Test() {
  TestRunCommand("/bin/echo Hi!*", 0);
  TestRunCommand("/bin/echo Hi!* Bye thri", 0);

  TestVersion("12.13.2.4", "12.13.2.4", false);   // 4 digit ==
  TestVersion("12.13.2.3", "12.13.2.4", true);    // 4 digit <
  TestVersion("12.13.2.4", "12.13.2.3", false);   // 4 digit >
  TestVersion("12.13.2", "12.13.2", false);       // 3 digit ==
  TestVersion("12.13.1", "12.13.2", true);        // 3 digit <
  TestVersion("12.13.4", "12.13.3", false);       // 3 digit >
  TestVersion("12.13.2", "12.14.1", true);        // 3 digit >
  TestVersion("12.13.2", "1.13.2.4", false);      // 3 digit, 4 digit
  TestVersion("12.13.2.4", "12.13.1", true);      // 4 digit, 3 digit

  TestReadFileToString("/tmp", false, NULL);
  TestReadFileToString("/foo", false, NULL);
  TestReadFileToString("lsb-release-test.txt", true, LSB_CONTENTS);
  TestReadFileToString("LICENSE", true, NULL);

  TestWriteStringToFile("fuzzy", "/tmp", false);
  TestWriteStringToFile("fuzzy", "/fuzzy/wuzzy", false);
  TestWriteStringToFile("fuzzy", "/tmp/fuzzy", true);
  TestWriteStringToFile("foobar", "/tmp/fuzzy", true);

  TestCopyFile("/tmp", "/tmp/wuzzy", false);
  TestCopyFile("/tmp/unknown_file", "/tmp/wuzzy", false);
  TestCopyFile("/tmp/fuzzy", "/tmp", false);
  TestCopyFile("/tmp/fuzzy", "/tmp/wuzzy", true);
  TestCopyFile("/tmp/fuzzy", "/tmp/wuzzy", true);
  TestCopyFile("LICENSE", "/tmp/LICENSE", true);

  // Cleanup temp files
  unlink("/tmp/fuzzy");
  unlink("/tmp/wuzzy");
  unlink("/tmp/LICENSE");

  TestLsb("bogus",
          "CHROMEOS_RELEASE_BOARD",
          false, NULL);
  TestLsb("lsb-release-test.txt",
          "CHROMEOS_RELEASE_BOARD",
          true, "x86-mario");
  TestLsb("lsb-release-test.txt",
          "CHROMEOS_RELEASE",
          true, "1568.0.2012_01_19_1424");
  TestLsb("lsb-release-test.txt",
          "CHROMEOS_AUSERVER",
          true, "http://blah.blah:8080/update");

  TestReplaceInFile("was",
                    "wuz",
                    "/fuzzy/wuzzy",  // doesn't exist, can't be created
                    "Fuzzy Wuzzy was a lamb",
                    false,
                    "Fuzzy Wuzzy wuz a lamb");

  TestReplaceInFile("was",
                    "wuz",
                    "/tmp/fuzzy",
                    "Fuzzy Wuzzy was a lamb",
                    true,
                    "Fuzzy Wuzzy wuz a lamb");

  TestReplaceInFile("wasn't",
                    "wuz",
                    "/tmp/fuzzy",
                    "Fuzzy Wuzzy wasn't a lamb",
                    true,
                    "Fuzzy Wuzzy wuz a lamb");

  TestReplaceInFile("AFuzzy",
                    "Fuzzy",
                    "/tmp/fuzzy",
                    "AFuzzy Wuzzy wuz a lamb",
                    true,
                    "Fuzzy Wuzzy wuz a lamb");

  TestReplaceInFile("lam",
                    "lamb",
                    "/tmp/fuzzy",
                    "Fuzzy Wuzzy wuz a lam",
                    true,
                    "Fuzzy Wuzzy wuz a lamb");

  // Cleanup temp files
  unlink("/tmp/fuzzy");

  TestGetBlockDevFromPartitionDev("/dev/sda3", "/dev/sda");
  TestGetBlockDevFromPartitionDev("/dev/sda321", "/dev/sda");
  TestGetBlockDevFromPartitionDev("/dev/sda", "/dev/sda");
  TestGetBlockDevFromPartitionDev("/dev/mmcblk0p3", "/dev/mmcblk0");
  TestGetBlockDevFromPartitionDev("/dev/mmcblk12p321", "/dev/mmcblk12");
  TestGetBlockDevFromPartitionDev("/dev/mmcblk0", "/dev/mmcblk0");
  TestGetBlockDevFromPartitionDev("", "");

  TestGetPartitionFromPartitionDev("/dev/sda3", 3);
  TestGetPartitionFromPartitionDev("/dev/sda321", 321);
  TestGetPartitionFromPartitionDev("/dev/sda", 0);
  TestGetPartitionFromPartitionDev("/dev/mmcblk0p3", 3);
  TestGetPartitionFromPartitionDev("/dev/mmcblk12p321", 321);
  TestGetPartitionFromPartitionDev("/dev/mmcblk1", 0);
  TestGetPartitionFromPartitionDev("3", 3);
  TestGetPartitionFromPartitionDev("", 0);

  TestMakePartitionDev("/dev/sda", 3, "/dev/sda3");
  TestMakePartitionDev("/dev/sda", 321, "/dev/sda321");
  TestMakePartitionDev("/dev/mmcblk0", 3, "/dev/mmcblk0p3");
  TestMakePartitionDev("/dev/mmcblk12", 321, "/dev/mmcblk12p321");
  TestMakePartitionDev("", 0, "0");

  TestDirname("/mnt/dir/postinst", "/mnt/dir");
  TestDirname("/mnt/dir/", "/mnt/dir");
  TestDirname("file", "");
  TestDirname("/", "");
  TestDirname("", "");

  TestTouch("/tmp/foo");
  TestTouch("/tmp/foo");
  TestTouch("/tmp/foopack");
  TestTouch("/tmp/foo.pack");

  TestRemovePackFiles("/tmp");
  TestRemovePackFiles("/");
  // Only "/tmp/foo" should be left

  TestR10FileSystemPatch("/tmp/foo");

  // This test really makes a block device readonly....
  // TestMakeDeviceReadOnly("/dev/sdc1");

  // Clean up final file
  unlink("/tmp/foo");

  string kernel_config =
      "root=/dev/dm-1 dm=\"foo bar, ver=2 root2=1 stuff=v\""
      " fuzzy=wuzzy root2=/dev/dm-2";
  string dm_config = "foo bar, ver=2 root2=1 stuff=v";

  TestExtractKernelArg(kernel_config, "root", "/dev/dm-1");
  TestExtractKernelArg(kernel_config, "root2", "/dev/dm-2");
  TestExtractKernelArg(kernel_config, "dm", dm_config);
  TestExtractKernelArg("root=\"", "root", "");
  TestExtractKernelArg("root=\" bar", "root", "");
  TestExtractKernelArg(dm_config, "ver", "2");
  TestExtractKernelArg(dm_config, "stuff", "v");

  TestSetKernelArg(kernel_config, "fuzzy", "tuzzy", true,
                   "root=/dev/dm-1 dm=\"foo bar, ver=2 root2=1 stuff=v\""
                   " fuzzy=tuzzy root2=/dev/dm-2");
  TestSetKernelArg(kernel_config, "root", "", true,
                   "root= dm=\"foo bar, ver=2 root2=1 stuff=v\""
                   " fuzzy=wuzzy root2=/dev/dm-2");
  TestSetKernelArg(kernel_config, "fuzzy", "tuzzy", true,
                   "root=/dev/dm-1 dm=\"foo bar, ver=2 root2=1 stuff=v\""
                   " fuzzy=tuzzy root2=/dev/dm-2");
  TestSetKernelArg(kernel_config, "root", "a b", true,
                   "root=\"a b\" dm=\"foo bar, ver=2 root2=1 stuff=v\""
                   " fuzzy=wuzzy root2=/dev/dm-2");
  TestSetKernelArg(kernel_config, "dm", "ab", true,
                   "root=/dev/dm-1 dm=ab fuzzy=wuzzy root2=/dev/dm-2");
  TestSetKernelArg(kernel_config, "dm", "a b", true,
                   "root=/dev/dm-1 dm=\"a b\" fuzzy=wuzzy root2=/dev/dm-2");
  TestSetKernelArg(kernel_config, "unknown", "", false, "");
  TestSetKernelArg(kernel_config, "ver", "", false, "");

  std::vector<std::string> expected;

  expected.clear();
  expected.push_back("My Stuff");
  TestSplitString("My Stuff", ',', expected);

  expected.clear();
  expected.push_back("My");
  expected.push_back("Stuff");
  expected.push_back("Is");
  TestSplitString("My,Stuff,Is", ',', expected);

  expected.clear();
  expected.push_back("");
  expected.push_back("My");
  expected.push_back("Stuff");
  TestSplitString(",My,Stuff", ',', expected);

  expected.clear();
  expected.push_back("My");
  expected.push_back("Stuff");
  expected.push_back("");
  TestSplitString("My,Stuff,", ',', expected);

  TestStringPrintf(StringPrintf(""), "");
  TestStringPrintf(StringPrintf("Stuff"), "Stuff");
  TestStringPrintf(StringPrintf("%s", "Stuff"), "Stuff");
  TestStringPrintf(StringPrintf("%d", 3), "3");
  TestStringPrintf(StringPrintf("%s %d", "Stuff", 3), "Stuff 3");

  TestConfigureInstall("/dev/sda3", "/mnt",
                       true,
                       "A", "/dev/sda3", "/dev/sda2", "/dev/sda12");
  TestConfigureInstall("/dev/sda5", "/mnt",
                       true,
                       "B", "/dev/sda5", "/dev/sda4", "/dev/sda12");
  TestConfigureInstall("/dev/mmcblk0p3", "/mnt",
                       true, "A",
                       "/dev/mmcblk0p3", "/dev/mmcblk0p2", "/dev/mmcblk0p12");
  TestConfigureInstall("/dev/mmcblk0p5", "/mnt",
                       true, "B",
                       "/dev/mmcblk0p5", "/dev/mmcblk0p4", "/dev/mmcblk0p12");
  TestConfigureInstall("/dev/sda2", "/mnt",
                       false, "", "", "", "");
  TestConfigureInstall("/dev/sda", "/mnt",
                       false, "", "", "", "");


  printf("SUCCESS!\n");
}
