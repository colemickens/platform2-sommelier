// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// How to build and run the tests:
//
// chroot$ cros_run_unit_tests --board=$BOARD --packages arc-setup
//
// Note: only x86 $BOARDs like cyan are supported.

#include "arc/setup/arc_setup_util.h"

#include <fcntl.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/bind.h>
#include <base/environment.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/rand_util.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <base/timer/elapsed_timer.h>
#include <chromeos-config/libcros_config/fake_cros_config.h>
#include <gtest/gtest.h>

namespace arc {

namespace {

bool FindLineCallback(const std::string& line, std::string* out_prop) {
  if (line != "string_to_find")
    return false;
  *out_prop = "FOUND";
  return true;
}

bool IsNonBlockingFD(int fd) {
  return fcntl(fd, F_GETFL) & O_NONBLOCK;
}

void ValidateResourcesMatch(const base::FilePath& path1,
                            const base::FilePath& path2) {
  struct stat stat1;
  struct stat stat2;
  EXPECT_GE(lstat(path1.value().c_str(), &stat1), 0);
  EXPECT_GE(lstat(path2.value().c_str(), &stat2), 0);
  EXPECT_EQ(stat1.st_mode, stat2.st_mode);
  EXPECT_EQ(stat1.st_uid, stat2.st_uid);
  EXPECT_EQ(stat1.st_gid, stat2.st_gid);

  if (S_ISREG(stat1.st_mode)) {
    std::string data1;
    std::string data2;
    EXPECT_TRUE(base::ReadFileToString(path1, &data1));
    EXPECT_TRUE(base::ReadFileToString(path2, &data2));
    EXPECT_EQ(data1, data2);
  } else if (S_ISLNK(stat1.st_mode)) {
    base::FilePath link1;
    base::FilePath link2;
    EXPECT_TRUE(base::ReadSymbolicLink(path1, &link1));
    EXPECT_TRUE(base::ReadSymbolicLink(path2, &link2));
    EXPECT_EQ(link1, link2);
  }
}

}  // namespace

TEST(ArcSetupUtil, TestEnvironmentUtils) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  const uint64_t random = base::RandUint64();
  const std::string random_var =
      base::StringPrintf("ARC_SETUP_UTIL_UNITTEST_%" PRIu64 "", random);

  EXPECT_FALSE(env->HasVar(random_var.c_str()));
  EXPECT_DEATH(GetEnvOrDie(env.get(), random_var.c_str()), ".*");
  EXPECT_DEATH(GetFilePathOrDie(env.get(), random_var.c_str()), ".*");

  const base::FilePath test_path("/tmp");
  EXPECT_TRUE(env->SetVar(random_var.c_str(), test_path.value()));

  EXPECT_EQ(test_path.value(), GetEnvOrDie(env.get(), random_var.c_str()));
  EXPECT_EQ(test_path.value(),
            GetFilePathOrDie(env.get(), random_var.c_str()).value());

  EXPECT_TRUE(env->SetVar(random_var.c_str(), "0"));
  EXPECT_FALSE(GetBooleanEnvOrDie(env.get(), random_var.c_str()));
  EXPECT_TRUE(env->SetVar(random_var.c_str(), "1"));
  EXPECT_TRUE(GetBooleanEnvOrDie(env.get(), random_var.c_str()));
  // Unexpected values evaluate to false.
  EXPECT_TRUE(env->SetVar(random_var.c_str(), "2"));
  EXPECT_FALSE(GetBooleanEnvOrDie(env.get(), random_var.c_str()));
  EXPECT_TRUE(env->SetVar(random_var.c_str(), "bad value"));
  EXPECT_FALSE(GetBooleanEnvOrDie(env.get(), random_var.c_str()));
  EXPECT_TRUE(env->SetVar(random_var.c_str(), ""));
  EXPECT_FALSE(GetBooleanEnvOrDie(env.get(), random_var.c_str()));

  env->UnSetVar(random_var.c_str());
}

TEST(ArcSetupUtil, TestCreateOrTruncate) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  // Create a new empty file.
  EXPECT_TRUE(CreateOrTruncate(temp_directory.GetPath().Append("file"), 0777));
  // Confirm that the mode of the file is 0777.
  int mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("file"), &mode));
  EXPECT_EQ(0777, mode);
  // Confirm that the size of the file is 0.
  int64_t size = -1;
  EXPECT_TRUE(
      base::GetFileSize(temp_directory.GetPath().Append("file"), &size));
  EXPECT_EQ(0, size);

  // Make the file non-empty.
  EXPECT_TRUE(
      WriteToFile(temp_directory.GetPath().Append("file"), 0777, "abc"));
  EXPECT_TRUE(
      base::GetFileSize(temp_directory.GetPath().Append("file"), &size));
  EXPECT_EQ(3, size);

  // Call the API again with a different mode.
  EXPECT_TRUE(CreateOrTruncate(temp_directory.GetPath().Append("file"), 0700));
  // Confirm that the mode of the file is now 0700.
  mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("file"), &mode));
  EXPECT_EQ(0700, mode);
  // Confirm that the size of the file is still 0.
  size = -1;
  EXPECT_TRUE(
      base::GetFileSize(temp_directory.GetPath().Append("file"), &size));
  EXPECT_EQ(0, size);
}

TEST(ArcSetupUtil, TestWaitForPaths) {
  constexpr base::TimeDelta timeout = base::TimeDelta::FromSeconds(1);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  // Confirm that when the first argument is empty, it returns true.
  // Also confirm that the third argument can be nullptr.
  EXPECT_TRUE(WaitForPaths({}, timeout, nullptr));

  // Confirm that the function can handle one path.
  base::TimeDelta elapsed;
  EXPECT_TRUE(WaitForPaths({temp_dir.GetPath()}, timeout, &elapsed));
  EXPECT_GT(elapsed, base::TimeDelta());
  // Strictly speaking, WaitForPaths does not guarantee this, but in practice,
  // this check passes.
  EXPECT_LE(elapsed, timeout);
  elapsed = base::TimeDelta();

  // Does the same with a nonexistent file.
  EXPECT_FALSE(WaitForPaths({temp_dir.GetPath().Append("nonexistent")}, timeout,
                            &elapsed));
  EXPECT_GT(elapsed, timeout);
  elapsed = base::TimeDelta();

  // Confirm that the function can handle two paths.
  EXPECT_TRUE(WaitForPaths({temp_dir.GetPath(), temp_dir2.GetPath()}, timeout,
                           &elapsed));
  EXPECT_GT(elapsed, base::TimeDelta());
  EXPECT_LE(elapsed, timeout);  // same
  elapsed = base::TimeDelta();

  EXPECT_FALSE(WaitForPaths(
      {temp_dir.GetPath().Append("nonexistent"), temp_dir2.GetPath()}, timeout,
      &elapsed));
  EXPECT_GT(elapsed, timeout);
  elapsed = base::TimeDelta();

  EXPECT_FALSE(WaitForPaths(
      {temp_dir.GetPath(), temp_dir2.GetPath().Append("nonexistent")}, timeout,
      &elapsed));
  EXPECT_GT(elapsed, timeout);
  elapsed = base::TimeDelta();

  EXPECT_FALSE(WaitForPaths({temp_dir.GetPath().Append("nonexistent"),
                             temp_dir2.GetPath().Append("nonexistent")},
                            timeout, &elapsed));
  EXPECT_GT(elapsed, timeout);
}

TEST(ArcSetupUtil, TestWriteToFile) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  // Create a non-empty file.
  EXPECT_TRUE(
      WriteToFile(temp_directory.GetPath().Append("file"), 0700, "abcde"));
  // Confirm that the mode of the file is now 0700.
  int mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("file"), &mode));
  EXPECT_EQ(0700, mode);
  // Confirm that the size of the file is still 0.
  int64_t size = -1;
  EXPECT_TRUE(
      base::GetFileSize(temp_directory.GetPath().Append("file"), &size));
  EXPECT_EQ(5, size);

  // Call the API again with a different mode and content.
  EXPECT_TRUE(
      WriteToFile(temp_directory.GetPath().Append("file"), 0777, "xyz"));
  // Confirm that the mode of the file is now 0700.
  mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("file"), &mode));
  EXPECT_EQ(0777, mode);
  // Confirm that the size of the file is still 0.
  size = -1;
  EXPECT_TRUE(
      base::GetFileSize(temp_directory.GetPath().Append("file"), &size));
  EXPECT_EQ(3, size);
}

TEST(ArcSetupUtil, TestWriteToFileWithSymlink) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::ScopedTempDir temp_directory2;
  ASSERT_TRUE(temp_directory2.CreateUniqueTempDir());

  const base::FilePath symlink = temp_directory.GetPath().Append("symlink");
  ASSERT_TRUE(base::CreateSymbolicLink(temp_directory2.GetPath(), symlink));

  // WriteToFile should fail when the path points to a symlink.
  EXPECT_FALSE(WriteToFile(symlink, 0777, "abc"));

  // WriteToFile should also fail when a path component in the middle is a
  // symlink.
  const base::FilePath path_with_symlink = symlink.Append("not-a-symlink");
  EXPECT_FALSE(WriteToFile(path_with_symlink, 0777, "abcde"));
}

TEST(ArcSetupUtil, TestWriteToFileWithFifo) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath fifo = temp_directory.GetPath().Append("fifo");
  ASSERT_EQ(0, mkfifo(fifo.value().c_str(), 0700));

  // WriteToFile should fail when the path points to a fifo.
  EXPECT_FALSE(WriteToFile(fifo, 0777, "abc"));
}

TEST(ArcSetupUtil, TestGetPropertyFromFile) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath prop_file = temp_directory.GetPath().Append("test.prop");

  // Create a new prop file and read it.
  ASSERT_TRUE(WriteToFile(prop_file, 0700, "key=val"));
  std::string v;
  EXPECT_TRUE(GetPropertyFromFile(prop_file, "key", &v));
  EXPECT_EQ("val", v);
  EXPECT_FALSE(GetPropertyFromFile(prop_file, "k", &v));
  EXPECT_FALSE(GetPropertyFromFile(prop_file, "ke", &v));
  EXPECT_FALSE(GetPropertyFromFile(prop_file, "keyX", &v));

  // Retry with an empty file.
  ASSERT_TRUE(WriteToFile(prop_file, 0700, ""));
  EXPECT_FALSE(GetPropertyFromFile(prop_file, "", &v));
  EXPECT_FALSE(GetPropertyFromFile(prop_file, "key", &v));

  // Retry with a multi-line file.
  ASSERT_TRUE(WriteToFile(prop_file, 0700, "k1=v1\nk2=v2\nk3=v3"));
  EXPECT_TRUE(GetPropertyFromFile(prop_file, "k1", &v));
  EXPECT_EQ("v1", v);
  EXPECT_TRUE(GetPropertyFromFile(prop_file, "k2", &v));
  EXPECT_EQ("v2", v);
  EXPECT_TRUE(GetPropertyFromFile(prop_file, "k3", &v));
  EXPECT_EQ("v3", v);
  EXPECT_FALSE(GetPropertyFromFile(prop_file, "", &v));
  EXPECT_FALSE(GetPropertyFromFile(prop_file, "=", &v));
  EXPECT_FALSE(GetPropertyFromFile(prop_file, "1", &v));
  EXPECT_FALSE(GetPropertyFromFile(prop_file, "k", &v));
  EXPECT_FALSE(GetPropertyFromFile(prop_file, "k4", &v));
}

TEST(ArcSetupUtil, TestGetFingerprintFromPackagesXml) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath packages_file =
      temp_directory.GetPath().Append("packages.xml");

  // Create a new file and read it.
  ASSERT_TRUE(WriteToFile(
      packages_file, 0700,
      "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"
      "<packages>\n"
      "  <version sdkVersion=\"25\" databaseVersion=\"3\" fingerprint=\"f1\">\n"
      "  <version volumeUuid=\"primary_physical\" "
      "sdkVersion=\"25\" databaseVersion=\"25\" fingerprint=\"f2\">\n"
      "</packages>"));
  std::string fingerprint;
  EXPECT_TRUE(GetFingerprintFromPackagesXml(packages_file, &fingerprint));
  EXPECT_EQ("f1", fingerprint);

  ASSERT_TRUE(WriteToFile(
      packages_file, 0700,
      // Reverse the order of the version elements.
      "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"
      "<packages>\n"
      "  <version volumeUuid=\"primary_physical\" "
      "sdkVersion=\"25\" databaseVersion=\"25\" fingerprint=\"f2\">\n"
      "  <version sdkVersion=\"25\" databaseVersion=\"3\" fingerprint=\"f1\">\n"
      "</packages>"));
  fingerprint.clear();
  EXPECT_TRUE(GetFingerprintFromPackagesXml(packages_file, &fingerprint));
  EXPECT_EQ("f1", fingerprint);

  // Test invalid <version>s.
  ASSERT_TRUE(WriteToFile(
      packages_file, 0700,
      // "external" version only.
      "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"
      "<packages>\n"
      "  <version volumeUuid=\"primary_physical\" "
      "sdkVersion=\"25\" databaseVersion=\"25\" fingerprint=\"f2\">\n"
      "</packages>"));
  EXPECT_FALSE(GetFingerprintFromPackagesXml(packages_file, &fingerprint));

  ASSERT_TRUE(
      WriteToFile(packages_file, 0700,
                  // No sdkVersion.
                  "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"
                  "<packages>\n"
                  "  <version databaseVersion=\"3\" fingerprint=\"f1\">\n"
                  "</packages>"));
  EXPECT_FALSE(GetFingerprintFromPackagesXml(packages_file, &fingerprint));

  ASSERT_TRUE(
      WriteToFile(packages_file, 0700,
                  // No databaseVersion.
                  "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"
                  "<packages>\n"
                  "  <version sdkVersion=\"25\" fingerprint=\"f1\">\n"
                  "</packages>"));
  EXPECT_FALSE(GetFingerprintFromPackagesXml(packages_file, &fingerprint));

  ASSERT_TRUE(
      WriteToFile(packages_file, 0700,
                  // No fingerprint.
                  "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"
                  "<packages>\n"
                  "  <version sdkVersion=\"25\" databaseVersion=\"3\">\n"
                  "</packages>"));
  EXPECT_FALSE(GetFingerprintFromPackagesXml(packages_file, &fingerprint));

  ASSERT_TRUE(WriteToFile(
      packages_file, 0700,
      // No valid fingerprint.
      "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"
      "<packages>\n"
      "  <version sdkVersion=\"25\" databaseVersion=\"3\" fingerprint=\"X>\n"
      "</packages>"));
  EXPECT_FALSE(GetFingerprintFromPackagesXml(packages_file, &fingerprint));

  ASSERT_TRUE(
      WriteToFile(packages_file, 0700,
                  // No <version> elements.
                  "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"
                  "<packages/>\n"));
  EXPECT_FALSE(GetFingerprintFromPackagesXml(packages_file, &fingerprint));

  ASSERT_TRUE(WriteToFile(packages_file, 0700,
                          // Empty file.
                          ""));
  EXPECT_FALSE(GetFingerprintFromPackagesXml(packages_file, &fingerprint));
}

// Tests the internal function directly.
TEST(ArcSetupUtil, TestFindLine) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath file = temp_directory.GetPath().Append("test.file");

  // Create a new prop file and read it.
  ASSERT_TRUE(WriteToFile(file, 0700, "string_to_find"));
  std::string v;
  EXPECT_TRUE(FindLineForTesting(file, base::Bind(&FindLineCallback), &v));
  EXPECT_EQ("FOUND", v);

  // Test with multi-line files.
  v.clear();
  ASSERT_TRUE(WriteToFile(file, 0700, "string_to_find\nline"));
  EXPECT_TRUE(FindLineForTesting(file, base::Bind(&FindLineCallback), &v));
  EXPECT_EQ("FOUND", v);
  v.clear();
  ASSERT_TRUE(WriteToFile(file, 0700, "line\nstring_to_find\nline"));
  EXPECT_TRUE(FindLineForTesting(file, base::Bind(&FindLineCallback), &v));
  EXPECT_EQ("FOUND", v);
  v.clear();
  ASSERT_TRUE(WriteToFile(file, 0700, "line\nstring_to_find"));
  EXPECT_TRUE(FindLineForTesting(file, base::Bind(&FindLineCallback), &v));
  EXPECT_EQ("FOUND", v);
  v.clear();
  ASSERT_TRUE(WriteToFile(file, 0700, "line\nstring_to_find\n"));
  EXPECT_TRUE(FindLineForTesting(file, base::Bind(&FindLineCallback), &v));
  EXPECT_EQ("FOUND", v);

  // Test without the target string.
  ASSERT_TRUE(WriteToFile(file, 0700, "string_to_findX"));
  EXPECT_FALSE(FindLineForTesting(file, base::Bind(&FindLineCallback), &v));
  ASSERT_TRUE(WriteToFile(file, 0700, "string_to_fin"));
  EXPECT_FALSE(FindLineForTesting(file, base::Bind(&FindLineCallback), &v));
  ASSERT_TRUE(WriteToFile(file, 0700, "string_to_fin\nd"));
  EXPECT_FALSE(FindLineForTesting(file, base::Bind(&FindLineCallback), &v));
  ASSERT_TRUE(WriteToFile(file, 0700, "s\ntring_to_find"));
  EXPECT_FALSE(FindLineForTesting(file, base::Bind(&FindLineCallback), &v));
  ASSERT_TRUE(WriteToFile(file, 0700, ""));
  EXPECT_FALSE(FindLineForTesting(file, base::Bind(&FindLineCallback), &v));
}

TEST(ArcSetupUtil, TestMkdirRecursively) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  // Set |temp_directory| to 0707.
  EXPECT_TRUE(base::SetPosixFilePermissions(temp_directory.GetPath(), 0707));

  EXPECT_TRUE(MkdirRecursively(temp_directory.GetPath().Append("a/b/c")));
  // Confirm the 3 directories are there.
  EXPECT_TRUE(base::DirectoryExists(temp_directory.GetPath().Append("a")));
  EXPECT_TRUE(base::DirectoryExists(temp_directory.GetPath().Append("a/b")));
  EXPECT_TRUE(base::DirectoryExists(temp_directory.GetPath().Append("a/b/c")));

  // Confirm that the newly created directories have 0755 mode.
  int mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("a"), &mode));
  EXPECT_EQ(0755, mode);
  mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("a/b"), &mode));
  EXPECT_EQ(0755, mode);
  mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("a/b/c"), &mode));
  EXPECT_EQ(0755, mode);

  // Confirm that the existing directory |temp_directory| still has 0707 mode.
  mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(temp_directory.GetPath(), &mode));
  EXPECT_EQ(0707, mode);

  // Call the API again which should still succeed.
  EXPECT_TRUE(MkdirRecursively(temp_directory.GetPath().Append("a/b/c")));
  EXPECT_TRUE(MkdirRecursively(temp_directory.GetPath().Append("a/b/c/d")));
  EXPECT_TRUE(
      base::DirectoryExists(temp_directory.GetPath().Append("a/b/c/d")));
  mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("a/b/c/d"), &mode));
  EXPECT_EQ(0755, mode);

  // Call the API again which should still succeed.
  EXPECT_TRUE(MkdirRecursively(temp_directory.GetPath().Append("a/b")));
  EXPECT_TRUE(MkdirRecursively(temp_directory.GetPath().Append("a")));

  // Try to create an existing directory ("/") should still succeed.
  EXPECT_TRUE(MkdirRecursively(base::FilePath("/")));

  // Try to pass a relative or empty directory. They should all fail.
  EXPECT_FALSE(MkdirRecursively(base::FilePath("foo")));
  EXPECT_FALSE(MkdirRecursively(base::FilePath("bar/")));
  EXPECT_FALSE(MkdirRecursively(base::FilePath()));
}

TEST(ArcSetupUtil, TestInstallDirectory) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  // Set |temp_directory| to 0707.
  EXPECT_TRUE(base::SetPosixFilePermissions(temp_directory.GetPath(), 0707));

  // Create a new directory.
  EXPECT_TRUE(InstallDirectory(0777, getuid(), getgid(),
                               temp_directory.GetPath().Append("sub1/sub2")));
  // Confirm that the mode for sub2 is 0777.
  int mode_sub2 = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("sub1/sub2"), &mode_sub2));
  EXPECT_EQ(0777, mode_sub2);
  // Confirm that the mode for sub1 is NOT 0777 but the secure default, 0755.
  int mode_sub1 = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("sub1"), &mode_sub1));
  EXPECT_EQ(0755, mode_sub1);

  // Confirm that the existing directory |temp_directory| still has 0707 mode.
  int mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(temp_directory.GetPath(), &mode));
  EXPECT_EQ(0707, mode);

  // Call InstallDirectory again with the same path but a different mode, 01700.
  EXPECT_TRUE(InstallDirectory(0700 | S_ISVTX, getuid(), getgid(),
                               temp_directory.GetPath().Append("sub1/sub2")));
  // Confirm that the mode for sub2 is now 01700.
  struct stat st;
  EXPECT_EQ(
      0,
      stat(temp_directory.GetPath().Append("sub1/sub2").value().c_str(), &st));
  EXPECT_EQ(0700 | S_ISVTX, st.st_mode & ~S_IFMT);
  mode_sub2 = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("sub1/sub2"), &mode_sub2));
  EXPECT_EQ(0700, mode_sub2);  // base's function masks the mode with 0777.
  // Confirm that the mode for sub1 is still 0755.
  mode_sub1 = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(
      temp_directory.GetPath().Append("sub1"), &mode_sub1));
  EXPECT_EQ(0755, mode_sub1);
  // Confirm that the existing directory |temp_directory| still has 0707 mode.
  mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(temp_directory.GetPath(), &mode));
  EXPECT_EQ(0707, mode);
}

TEST(ArcSetupUtil, TestInstallDirectoryWithSymlink) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::ScopedTempDir temp_directory2;
  ASSERT_TRUE(temp_directory2.CreateUniqueTempDir());

  const base::FilePath symlink = temp_directory.GetPath().Append("symlink");
  ASSERT_TRUE(base::CreateSymbolicLink(temp_directory2.GetPath(), symlink));

  // InstallDirectory should fail when the path points to a symlink.
  EXPECT_FALSE(InstallDirectory(0777, getuid(), getgid(), symlink));

  // InstallDirectory should also fail when a path component in the middle
  // is a symlink.
  const base::FilePath path_with_symlink = symlink.Append("not-a-symlink");
  EXPECT_FALSE(InstallDirectory(0777, getuid(), getgid(), path_with_symlink));
}

TEST(ArcSetupUtil, TestInstallDirectoryWithFifo) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath fifo = temp_directory.GetPath().Append("fifo");
  ASSERT_EQ(0, mkfifo(fifo.value().c_str(), 0700));

  // InstallDirectory should fail when the path points to a fifo.
  EXPECT_FALSE(InstallDirectory(0777, getuid(), getgid(), fifo));
}

TEST(ArcSetupUtil, TestDeleteFilesInDir) {
  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  ASSERT_TRUE(MkdirRecursively(directory.GetPath().Append("arm")));
  ASSERT_TRUE(MkdirRecursively(directory.GetPath().Append("arm64")));
  ASSERT_TRUE(CreateOrTruncate(
      directory.GetPath().Append("arm/system@framework@boot.art"), 0755));
  ASSERT_TRUE(CreateOrTruncate(
      directory.GetPath().Append("arm64/system@framework@boot.art"), 0755));
  EXPECT_TRUE(base::PathExists(
      directory.GetPath().Append("arm/system@framework@boot.art")));
  EXPECT_TRUE(base::PathExists(
      directory.GetPath().Append("arm/system@framework@boot.art")));

  EXPECT_TRUE(arc::DeleteFilesInDir(directory.GetPath()));

  EXPECT_TRUE(base::PathExists(directory.GetPath().Append("arm")));
  EXPECT_TRUE(base::PathExists(directory.GetPath().Append("arm64")));
  EXPECT_FALSE(base::PathExists(
      directory.GetPath().Append("arm/system@framework@boot.art")));
  EXPECT_FALSE(base::PathExists(
      directory.GetPath().Append("arm/system@framework@boot.art")));
}

TEST(ArcSetupUtil, TestLaunchAndWait) {
  base::ElapsedTimer timer;
  // Check that LaunchAndWait actually blocks until sleep returns.
  EXPECT_TRUE(LaunchAndWait({"/usr/bin/sleep", "1"}));
  EXPECT_LE(1, timer.Elapsed().InSeconds());

  EXPECT_FALSE(LaunchAndWait({"/bin/false"}));
  EXPECT_FALSE(LaunchAndWait({"/no_such_binary"}));
}

TEST(ArcSetupUtil, TestGenerateFakeSerialNumber) {
  // Check that the function always returns 20-character string.
  EXPECT_EQ(20U,
            GenerateFakeSerialNumber("mytestaccount@gmail.com", "001122aabbcc")
                .size());
  EXPECT_EQ(20U, GenerateFakeSerialNumber("", "").size());
  EXPECT_EQ(20U, GenerateFakeSerialNumber("a", "b").size());

  // Check that the function always returns the same ID for the same
  // account and hwid_raw.
  const std::string id_1 =
      GenerateFakeSerialNumber("mytestaccount@gmail.com", "001122aabbcc");
  const std::string id_2 =
      GenerateFakeSerialNumber("mytestaccount@gmail.com", "001122aabbcc");
  EXPECT_EQ(id_1, id_2);

  // Generate an ID for a different account but for the same machine.
  // Check that the ID is not the same as |id_1|.
  const std::string id_3 = GenerateFakeSerialNumber("mytestaccount2@gmail.com",
                                                    //            ^
                                                    "001122aabbcc");
  EXPECT_NE(id_1, id_3);

  // Generate an ID for a different machine but for the same account.
  // Check that the ID is not the same as |id_1|.
  const std::string id_4 =
      GenerateFakeSerialNumber("mytestaccount@gmail.com", "001122aaddcc");
  //                                                         ^^
  EXPECT_NE(id_1, id_4);

  // Check that the function treats '\0' in |salt| properly.
  const std::string id_5 =
      GenerateFakeSerialNumber("mytestaccount@gmail.com", {'a', '\0', 'b'});
  const std::string id_6 =
      GenerateFakeSerialNumber("mytestaccount@gmail.com", {'a', '\0', 'c'});
  EXPECT_NE(id_5, id_6);
}

TEST(ArcSetupUtil, TestGetArtCompilationOffsetSeed) {
  const uint64_t seed1 = GetArtCompilationOffsetSeed("salt1", "build1");
  const uint64_t seed2 = GetArtCompilationOffsetSeed("salt2", "build1");
  const uint64_t seed3 = GetArtCompilationOffsetSeed("salt1", "build2");
  EXPECT_NE(0ULL, seed1);
  EXPECT_NE(0ULL, seed2);
  EXPECT_NE(0ULL, seed3);
  EXPECT_NE(seed1, seed2);
  EXPECT_NE(seed2, seed3);
  EXPECT_NE(seed3, seed1);
}

TEST(ArcSetupUtil, TestMoveDataAppOatDirectory) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  base::ScopedTempDir temp_target_directory;
  ASSERT_TRUE(temp_target_directory.CreateUniqueTempDir());

  // Create cache files.
  ASSERT_TRUE(
      MkdirRecursively(temp_directory.GetPath().Append("com.a/oat/arm")));
  ASSERT_TRUE(
      MkdirRecursively(temp_directory.GetPath().Append("com.a/oat/arm64")));
  ASSERT_TRUE(CreateOrTruncate(
      temp_directory.GetPath().Append("com.a/oat/arm/a.dex"), 0755));
  ASSERT_TRUE(CreateOrTruncate(
      temp_directory.GetPath().Append("com.a/oat/arm64/a.dex"), 0755));
  ASSERT_TRUE(MkdirRecursively(temp_directory.GetPath().Append("com.a/apk")));

  EXPECT_TRUE(
      base::PathExists(temp_directory.GetPath().Append("com.a/oat/arm/a.dex")));
  EXPECT_TRUE(base::PathExists(
      temp_directory.GetPath().Append("com.a/oat/arm64/a.dex")));
  EXPECT_TRUE(base::PathExists(temp_directory.GetPath().Append("com.a/apk")));

  MoveDataAppOatDirectory(temp_directory.GetPath(),
                          temp_target_directory.GetPath());

  EXPECT_FALSE(
      base::PathExists(temp_directory.GetPath().Append("com.a/oat/arm/a.dex")));
  EXPECT_FALSE(base::PathExists(
      temp_directory.GetPath().Append("com.a/oat/arm64/a.dex")));
  EXPECT_FALSE(base::PathExists(temp_directory.GetPath().Append("com.a/oat")));
  EXPECT_TRUE(base::PathExists(temp_directory.GetPath().Append("com.a/apk")));

  EXPECT_TRUE(base::PathExists(
      temp_target_directory.GetPath().Append("oat-com.a/arm/a.dex")));
  EXPECT_TRUE(base::PathExists(
      temp_target_directory.GetPath().Append("oat-com.a/arm64/a.dex")));
}

TEST(ArcSetupUtil, TestGetChromeOsChannelFromFile) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath prop_file = temp_directory.GetPath().Append("test.prop");

  ASSERT_TRUE(
      WriteToFile(prop_file, 0700, "CHROMEOS_RELEASE_TRACK=beta-channel"));
  EXPECT_EQ("beta", GetChromeOsChannelFromFile(prop_file));

  ASSERT_TRUE(
      WriteToFile(prop_file, 0700, "CHROMEOS_RELEASE_TRACK=canary-channel"));
  EXPECT_EQ("canary", GetChromeOsChannelFromFile(prop_file));

  ASSERT_TRUE(
      WriteToFile(prop_file, 0700, "CHROMEOS_RELEASE_TRACK=dev-channel"));
  EXPECT_EQ("dev", GetChromeOsChannelFromFile(prop_file));

  ASSERT_TRUE(
      WriteToFile(prop_file, 0700, "CHROMEOS_RELEASE_TRACK=dogfood-channel"));
  EXPECT_EQ("dogfood", GetChromeOsChannelFromFile(prop_file));

  ASSERT_TRUE(
      WriteToFile(prop_file, 0700, "CHROMEOS_RELEASE_TRACK=stable-channel"));
  EXPECT_EQ("stable", GetChromeOsChannelFromFile(prop_file));

  ASSERT_TRUE(
      WriteToFile(prop_file, 0700, "CHROMEOS_RELEASE_TRACK=testimage-channel"));
  EXPECT_EQ("testimage", GetChromeOsChannelFromFile(prop_file));

  // Checked "unknown" is returned if no value is set
  ASSERT_TRUE(WriteToFile(prop_file, 0700, "CHROMEOS_RELEASE_TRACK="));
  EXPECT_EQ("unknown", GetChromeOsChannelFromFile(prop_file));

  // Checked "unknown" is returned if some unknown string is set
  ASSERT_TRUE(WriteToFile(prop_file, 0700, "CHROMEOS_RELEASE_TRACK=foo_bar"));
  EXPECT_EQ("unknown", GetChromeOsChannelFromFile(prop_file));

  // Checked "unknown" is returned if CHROMEOS_RELEASE_TRACK is not in the file
  ASSERT_TRUE(WriteToFile(prop_file, 0700, " "));
  EXPECT_EQ("unknown", GetChromeOsChannelFromFile(prop_file));

  // Checked "unknown" is returned if file is not present
  EXPECT_EQ("unknown", GetChromeOsChannelFromFile(base::FilePath("foo")));
}

TEST(ArcSetupUtil, TestParseContainerState) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath json_file = temp_directory.GetPath().Append("state.json");

  const base::FilePath kRootfsPath(
      "/opt/google/containers/android/rootfs/root");

  constexpr char kJsonTemplate[] = R"json(
    {
      "ociVersion": "1.0",
      "id": "android-container",
      "status": "created",
      "pid": 4422,
      "bundle": "/opt/google/containers/android",
      "annotations": {
        "org.chromium.run_oci.container_root": "%s"
      }
    }
  )json";

  ASSERT_TRUE(WriteToFile(
      json_file, 0700,
      base::StringPrintf(kJsonTemplate,
                         temp_directory.GetPath().value().c_str())));
  ASSERT_TRUE(MkdirRecursively(temp_directory.GetPath().Append("mountpoints")));
  ASSERT_TRUE(base::CreateSymbolicLink(
      kRootfsPath,
      temp_directory.GetPath().Append("mountpoints/container-root")));

  pid_t container_pid;
  base::FilePath rootfs;
  EXPECT_TRUE(GetOciContainerState(json_file, &container_pid, &rootfs));
  EXPECT_EQ(4422, container_pid);
  EXPECT_EQ(kRootfsPath, rootfs);
}

TEST(ArcSetupUtil, TestPropertyExpansions) {
  brillo::FakeCrosConfig config;
  config.SetString("/arc/build-properties", "brand", "alphabet");

  std::string expanded;
  EXPECT_TRUE(ExpandPropertyContents("line1\n{brand}\nline3\n{brand} {brand}",
                                     &config, &expanded));
  EXPECT_EQ("line1\nalphabet\nline3\nalphabet alphabet\n", expanded);
}

TEST(ArcSetupUtil, TestPropertyExpansionsUnmatchedBrace) {
  brillo::FakeCrosConfig config;
  config.SetString("/arc/build-properties", "brand", "alphabet");

  std::string expanded;
  EXPECT_FALSE(
      ExpandPropertyContents("line{1\nline}2\nline3", &config, &expanded));
}

TEST(ArcSetupUtil, TestPropertyExpansionsRecursive) {
  brillo::FakeCrosConfig config;
  config.SetString("/arc/build-properties", "brand", "alphabet");
  config.SetString("/arc/build-properties", "model", "{brand} soup");

  std::string expanded;
  EXPECT_TRUE(ExpandPropertyContents("{model}", &config, &expanded));
  EXPECT_EQ("alphabet soup\n", expanded);
}

TEST(ArcSetupUtil, TestPropertyExpansionsMissingProperty) {
  brillo::FakeCrosConfig config;
  config.SetString("/arc/build-properties", "model", "{brand} soup");

  std::string expanded;

  EXPECT_FALSE(
      ExpandPropertyContents("{missing-property}", &config, &expanded));
  EXPECT_FALSE(ExpandPropertyContents("{model}", &config, &expanded));
}

// Non-fingerprint property should do simple truncation.
TEST(ArcSetupUtil, TestPropertyTruncation) {
  std::string truncated = TruncateAndroidProperty(
      "property.name="
      "012345678901234567890123456789012345678901234567890123456789"
      "01234567890123456789012345678901");
  EXPECT_EQ(
      "property.name=0123456789012345678901234567890123456789"
      "012345678901234567890123456789012345678901234567890",
      truncated);
}

// Fingerprint truncation with /release-keys should do simple truncation.
TEST(ArcSetupUtil, TestPropertyTruncationFingerprintRelease) {
  std::string truncated = TruncateAndroidProperty(
      "ro.bootimage.build.fingerprint=google/toolongdevicename/"
      "toolongdevicename_cheets:7.1.1/R65-10299.0.9999/4538390:user/"
      "release-keys");
  EXPECT_EQ(
      "ro.bootimage.build.fingerprint=google/toolongdevicename/"
      "toolongdevicename_cheets:7.1.1/R65-10299.0.9999/4538390:user/relea",
      truncated);
}

// Fingerprint truncation with /dev-keys needs to preserve the /dev-keys.
TEST(ArcSetupUtil, TestPropertyTruncationFingerprintDev) {
  std::string truncated = TruncateAndroidProperty(
      "ro.bootimage.build.fingerprint=google/toolongdevicename/"
      "toolongdevicename_cheets:7.1.1/R65-10299.0.9999/4538390:user/dev-keys");
  EXPECT_EQ(
      "ro.bootimage.build.fingerprint=google/toolongdevicena/"
      "toolongdevicena_cheets/R65-10299.0.9999/4538390:user/dev-keys",
      truncated);
}

// Tests if the O_NONBLOCK removal feature is working well. Other part of the
// function is tested in TestInstallDirectory*.
TEST(ArcSetupUtil, TestOpenSafely) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath file = temp_directory.GetPath().Append("file");
  ASSERT_TRUE(CreateOrTruncate(file, 0700));

  base::ScopedFD fd(OpenSafelyForTesting(file, O_RDONLY, 0));
  EXPECT_TRUE(fd.is_valid());
  EXPECT_FALSE(IsNonBlockingFD(fd.get()));

  fd = OpenSafelyForTesting(file, O_RDONLY | O_NONBLOCK, 0);
  EXPECT_TRUE(fd.is_valid());
  EXPECT_TRUE(IsNonBlockingFD(fd.get()));
}

TEST(ArcSetupUtil, TestOpenFifoSafely) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath fifo = temp_directory.GetPath().Append("fifo");
  ASSERT_EQ(0, mkfifo(fifo.value().c_str(), 0700));
  const base::FilePath file = temp_directory.GetPath().Append("file");
  ASSERT_TRUE(CreateOrTruncate(file, 0700));

  base::ScopedFD fd(OpenFifoSafely(fifo, O_RDONLY, 0));
  EXPECT_TRUE(fd.is_valid());
  EXPECT_FALSE(IsNonBlockingFD(fd.get()));

  // Opening a file should fail.
  fd = OpenFifoSafely(file, O_RDONLY, 0);
  EXPECT_FALSE(fd.is_valid());
}

TEST(ArcSetupUtil, TestCopyWithAttributes) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  // Note, actual owner change is not covered due permission restrictions for
  // unit test. selinux context is also not possible to test due the
  // permissions.
  const uid_t kTestUid = getuid();
  const gid_t kTestGid = getgid();

  const base::FilePath root = temp_directory.GetPath();

  // Create test directory structure.
  const base::FilePath from_path = root.Append("from");
  const base::FilePath from_sub_dir1 = from_path.Append("dir1");
  const base::FilePath from_sub_dir2 = from_path.Append("dir2");
  const base::FilePath from_test_file = from_sub_dir1.Append("test.txt");
  const base::FilePath from_test_link = from_sub_dir2.Append("test.lnk");
  const base::FilePath from_fifo = from_sub_dir1.Append("fifo");

  EXPECT_TRUE(InstallDirectory(0751, kTestUid, kTestGid, from_path));
  EXPECT_TRUE(InstallDirectory(0771, kTestUid, kTestGid, from_sub_dir1));
  EXPECT_TRUE(InstallDirectory(0700, kTestUid, kTestGid, from_sub_dir2));
  EXPECT_TRUE(WriteToFile(from_test_file, 0660, "testme"));
  EXPECT_TRUE(base::CreateSymbolicLink(from_test_file, from_test_link));
  EXPECT_EQ(0, mkfifo(from_fifo.value().c_str(), 0700));

  // Copy directory.
  const base::FilePath to_path = root.Append("to");
  EXPECT_TRUE(CopyWithAttributes(from_path, to_path));

  // Validate each resource to match.
  int resource_count = 1;
  ValidateResourcesMatch(from_path, to_path);
  base::FileEnumerator traversal(from_path, true /* recursive */,
                                 base::FileEnumerator::FILES |
                                     base::FileEnumerator::SHOW_SYM_LINKS |
                                     base::FileEnumerator::DIRECTORIES);
  while (true) {
    const base::FilePath test = traversal.Next();
    if (test.empty())
      break;
    base::FilePath target_path(to_path);
    EXPECT_TRUE(from_path.AppendRelativePath(test, &target_path));
    if (test != from_fifo) {
      ValidateResourcesMatch(test, target_path);
      ++resource_count;
    } else {
      // Unsupported types.
      EXPECT_FALSE(base::PathExists(target_path));
    }
  }
  EXPECT_EQ(5, resource_count);

  // Copy file.
  const base::FilePath to_test_file = from_sub_dir2.Append("test2.txt");
  EXPECT_TRUE(CopyWithAttributes(from_test_file, to_test_file));
  ValidateResourcesMatch(from_test_file, to_test_file);
  EXPECT_TRUE(CopyWithAttributes(from_test_file, to_test_file));
  ValidateResourcesMatch(from_test_file, to_test_file);

  // Copy link.
  const base::FilePath to_test_link = from_sub_dir2.Append("test2.lnk");
  EXPECT_TRUE(CopyWithAttributes(from_test_link, to_test_link));
  ValidateResourcesMatch(from_test_file, to_test_file);

  // Copy fifo
  EXPECT_FALSE(CopyWithAttributes(from_fifo, from_sub_dir1.Append("fifo2")));
}

TEST(ArcSetupUtil, TestSetFingerprintForPackagesCache) {
  constexpr char kFingerintBefore[] =
      "<packages>\n"
      "    <version sdkVersion=\"25\" databaseVersion=\"3\" "
      "fingerprint=\"google/coral/{product}_cheets:7.1.1/R67-10545.0.0/"
      "4697494:user/release-keys\" />\n"
      "    <version volumeUuid=\"primary_physical\" sdkVersion=\"25\" "
      "databaseVersion=\"3\" fingerprint=\"google/coral/{product}_cheets:"
      "7.1.1/R67-10545.0.0/4697494:user/release-keys\" />\n"
      "</packages>\n";
  constexpr char kFingerintAfter[] =
      "<packages>\n"
      "    <version sdkVersion=\"25\" databaseVersion=\"3\" "
      "fingerprint=\"google/coral/coral_cheets:7.1.1/R67-10545.0.0/"
      "4697494:user/release-keys\" />\n"
      "    <version volumeUuid=\"primary_physical\" sdkVersion=\"25\" "
      "databaseVersion=\"3\" fingerprint=\"google/coral/coral_cheets:"
      "7.1.1/R67-10545.0.0/4697494:user/release-keys\" />\n"
      "</packages>\n";
  std::string new_content;
  SetFingerprintsForPackagesCache(
      kFingerintBefore,
      "google/coral/coral_cheets:7.1.1/R67-10545.0.0/4697494:user/release-keys",
      &new_content);
  EXPECT_EQ(strlen(kFingerintAfter), new_content.length());
  EXPECT_EQ(kFingerintAfter, new_content);
}

}  // namespace arc
