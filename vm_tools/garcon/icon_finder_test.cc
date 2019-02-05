// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <base/environment.h>

#include <base/files/scoped_temp_dir.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <gtest/gtest.h>

#include "vm_tools/garcon/icon_finder.cc"

namespace vm_tools {
namespace garcon {

namespace {

class IconFinderTest : public ::testing::Test {
 public:
  IconFinderTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    data_dir_ = temp_dir_.GetPath();
    desktop_file_dir_ = data_dir_.Append("applications");
    CHECK(base::CreateDirectory(desktop_file_dir_));
    icon_theme_dir_ = data_dir_.Append("icons").Append("hicolor");
    icon_dir_ = icon_theme_dir_.Append("48x48").Append("apps");
    CHECK(base::CreateDirectory(icon_dir_));
  }
  ~IconFinderTest() override = default;

  void WriteIndexThemeFile(const std::string& contents) {
    base::FilePath file_path = icon_theme_dir_.Append("index.theme");
    EXPECT_EQ(contents.size(),
              base::WriteFile(file_path, contents.c_str(), contents.size()));
  }

  void WriteDesktopFile(const std::string& desktop_file_name,
                        const std::string& contents) {
    base::FilePath file_path = desktop_file_dir_.Append(desktop_file_name);
    EXPECT_EQ(contents.size(),
              base::WriteFile(file_path, contents.c_str(), contents.size()));
  }

  const base::FilePath& icon_theme_dir() { return icon_theme_dir_; }
  const base::FilePath& icon_dir() { return icon_dir_; }
  const base::FilePath& data_dir() { return data_dir_; }
  void ValidateIconIndexDirsWithXDGDataDir(
      const std::string& xdg_data_dirs_value,
      const std::vector<base::FilePath>& expected_dirs) {
    std::unique_ptr<base::Environment> env = base::Environment::Create();
    env->SetVar("XDG_DATA_DIRS", xdg_data_dirs_value);
    EXPECT_TRUE(expected_dirs == GetPathsForIconIndexDirs());
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath data_dir_;
  base::FilePath desktop_file_dir_;
  base::FilePath icon_theme_dir_;
  base::FilePath icon_dir_;

  DISALLOW_COPY_AND_ASSIGN(IconFinderTest);
};

}  // namespace

// This test verifies that icon_finder uses environment variable XDG_DATA_DIRS
// to search for app icons when it's set.
TEST_F(IconFinderTest, UseXdgDataDirsEnv) {
  ValidateIconIndexDirsWithXDGDataDir(
      "/a:/b",
      {base::FilePath("/a/icons/gnome"), base::FilePath("/b/icons/gnome"),
       base::FilePath("/a/icons/hicolor"), base::FilePath("/b/icons/hicolor")});
}

// This test verifies that default XDG data directories are used when
// environment variable XDG_DATA_DIRS is not set.
TEST_F(IconFinderTest, DefaultDirs) {
  ValidateIconIndexDirsWithXDGDataDir(
      "", {base::FilePath("/usr/share/icons/gnome"),
           base::FilePath("/usr/share/icons/hicolor")});
}

// This test verifies that we get a default list of dirs when the index.theme
// file is missing.
TEST_F(IconFinderTest, NoIndexThemeNoDir) {
  std::vector<base::FilePath> expected_dirs = {
      icon_theme_dir().Append("48x48").Append("apps"),
      icon_theme_dir().Append("128x128").Append("apps"),
      icon_theme_dir().Append("96x96").Append("apps"),
      icon_theme_dir().Append("64x64").Append("apps"),
      icon_theme_dir().Append("32x32").Append("apps"),
  };
  EXPECT_TRUE(GetPathsForIcons(icon_theme_dir(), 48, 1) == expected_dirs);
}

// This test verifies that the correct icon dirs are returned.
TEST_F(IconFinderTest, CorrectDirsReturned) {
  WriteIndexThemeFile(
      "[Icon Theme]\n"
      "Name=Hicolor\n"
      "Comment=Fallback icon theme\n"
      "Hidden=true\n"
      "Directories=48x48/apps\n"
      "\n\n"
      "[48x48/apps]\n"
      "Size=48\n"
      "Context=Applications\n"
      "Type=Threshold\n\n");
  std::vector<base::FilePath> expected_dirs = {icon_dir()};
  EXPECT_TRUE(GetPathsForIcons(icon_theme_dir(), 48, 1) == expected_dirs);
}

// This test verifies that empty dir is returned when no desktop file exists.
TEST_F(IconFinderTest, NoDesktopfileNoDir) {
  EXPECT_TRUE(LocateIconFile("gimp.desktop", 48, 1) == base::FilePath());
}

// This test verifies that correct icon file path is returned.
TEST_F(IconFinderTest, HappyCase) {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  env->SetVar("XDG_DATA_DIRS", data_dir().value());
  WriteDesktopFile("gimp.desktop",
                   "[Desktop Entry]\n"
                   "Type=Application\n"
                   "Name=gimp\n"
                   "Icon=gimp");
  WriteIndexThemeFile(
      "[Icon Theme]\n"
      "Name=Hicolor\n"
      "Comment=Fallback icon theme\n"
      "Hidden=true\n"
      "Directories=48x48/apps\n"
      "\n\n"
      "[48x48/apps]\n"
      "Size=48\n"
      "Context=Applications\n"
      "Type=Threshold\n\n");
  base::FilePath icon_file_path = icon_dir().Append("gimp.png");
  base::WriteFile(icon_file_path, "", 0);
  EXPECT_TRUE(LocateIconFile("gimp", 48, 1) == icon_file_path);
}

}  // namespace garcon
}  // namespace vm_tools
