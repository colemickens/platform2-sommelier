// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_test_utils.h"

#include <stdio.h>
#include <stdlib.h>

#include "inst_util.h"

using std::string;

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

void TestLsb(const string &key, const string &expected) {
  string file = "lsb-release-test.txt";

  string result = LsbReleaseValue(file, key);
  if (result != expected) {
    printf("LsbReleaseValue(%s, %s) '%s' != %s\n",
           file.c_str(), key.c_str(),
           result.c_str(), expected.c_str());
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
    printf("ExtractKernelArg(%s, %s) '%s' != %s\n",
           kernel_config.c_str(), tag.c_str(),
           result.c_str(), expected.c_str());
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

  TestLsb("CHROMEOS_RELEASE_BOARD", "x86-mario");
  TestLsb("CHROMEOS_RELEASE", "1568.0.2012_01_19_1424");
  TestLsb("CHROMEOS_AUSERVER", "http://blah.blah:8080/update");

  TestGetBlockDevFromPartitionDev("/dev/sda3", "/dev/sda");
  TestGetBlockDevFromPartitionDev("/dev/sda321", "/dev/sda");
  TestGetBlockDevFromPartitionDev("", "");

  TestGetPartitionFromPartitionDev("/dev/sda3", 3);
  TestGetPartitionFromPartitionDev("/dev/sda321", 321);
  TestGetPartitionFromPartitionDev("/dev/sda", 0);
  TestGetPartitionFromPartitionDev("3", 3);
  TestGetPartitionFromPartitionDev("", 0);

  TestMakePartitionDev("/dev/sda", 3, "/dev/sda3");
  TestMakePartitionDev("/dev/sda", 321, "/dev/sda321");
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
      "root=/dev/dm-1 dm=\"foo bar, ver=2 root2=1 stuff=v\" root2=/dev/dm-2";
  string dm_config = "foo bar, ver=2 root2=1 stuff=v";

  TestExtractKernelArg(kernel_config, "root", "/dev/dm-1");
  TestExtractKernelArg(kernel_config, "root2", "/dev/dm-2");
  TestExtractKernelArg(kernel_config, "dm", dm_config);
  TestExtractKernelArg(dm_config, "ver", "2");
  TestExtractKernelArg(dm_config, "stuff", "v");

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

  printf("SUCCESS!\n");
}
