// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/scoped_temp_dir.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <gtest/gtest.h>

#include "vm_tools/garcon/desktop_file.h"

namespace vm_tools {
namespace garcon {

namespace {
struct DesktopFileTestData {
  std::string app_id;
  std::string entry_type;
  std::map<std::string, std::string> locale_name_map;
  std::map<std::string, std::string> locale_comment_map;
  bool no_display;
  std::string icon;
  bool hidden;
  std::vector<std::string> only_show_in;
  std::string try_exec;
  std::string exec;
  std::string path;
  bool terminal;
  std::vector<std::string> mime_types;
  std::vector<std::string> categories;
  std::string startup_wm_class;
};

class DesktopFileTest : public ::testing::Test {
 public:
  DesktopFileTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    apps_dir_ = temp_dir_.GetPath().Append("applications");
    CHECK(base::CreateDirectory(apps_dir_));
  }
  ~DesktopFileTest() override = default;

  void ValidateDesktopFile(std::string file_contents,
                           std::string relative_path,
                           const DesktopFileTestData& results,
                           bool expect_pass) {
    base::FilePath desktop_file_path = apps_dir_.Append(relative_path);
    // If there's a relative path, create any directories in it.
    CHECK(base::CreateDirectory(desktop_file_path.DirName()));
    EXPECT_EQ(file_contents.size(),
              base::WriteFile(desktop_file_path, file_contents.c_str(),
                              file_contents.size()));
    std::unique_ptr<DesktopFile> result =
        DesktopFile::ParseDesktopFile(desktop_file_path);
    if (!expect_pass) {
      EXPECT_EQ(nullptr, result.get());
      return;
    }
    EXPECT_EQ(result->app_id(), results.app_id);
    EXPECT_EQ(result->entry_type(), results.entry_type);
    EXPECT_EQ(result->locale_name_map(), results.locale_name_map);
    EXPECT_EQ(result->locale_comment_map(), results.locale_comment_map);
    EXPECT_EQ(result->no_display(), results.no_display);
    EXPECT_EQ(result->icon(), results.icon);
    EXPECT_EQ(result->hidden(), results.hidden);
    EXPECT_EQ(result->only_show_in(), results.only_show_in);
    EXPECT_EQ(result->try_exec(), results.try_exec);
    EXPECT_EQ(result->exec(), results.exec);
    EXPECT_EQ(result->path(), results.path);
    EXPECT_EQ(result->terminal(), results.terminal);
    EXPECT_EQ(result->mime_types(), results.mime_types);
    EXPECT_EQ(result->categories(), results.categories);
    EXPECT_EQ(result->startup_wm_class(), results.startup_wm_class);
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath apps_dir_;

  DISALLOW_COPY_AND_ASSIGN(DesktopFileTest);
};

}  // namespace

// This tests most parsing, comments, line breaks, multi-strings, simple
// locales and that all the keys we care about are parsed and invalid ones are
// ignored.
TEST_F(DesktopFileTest, AllKeys) {
  ValidateDesktopFile(
      "#Comment1\n"
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name=Test\n"
      "\n\n"
      "Name[fr]=Test French\n"
      "Comment=Test me out!\n"
      "Comment[es]=Hola for the comment\n"
      "#Comment2\n"
      "#Comment3\n"
      "NoDisplay=true\n"
      "Icon=prettyicon\n"
      "Hidden=true\n"
      "\n\n"
      "OnlyShowIn=KDE;Gnome;\n"
      "TryExec=mybinary\n"
      "UnknownKey=trickster\n"
      "Exec=mybinary %F\n"
      "#Comment4\n"
      "Path=/usr/local/bin\n"
      "Terminal=true\n"
      "MimeType=text/plain;foo/x-java\n"
      "Categories=Magic;Playtime\n"
      "StartupWMClass=classy\n",
      "test.desktop",
      {
          "test",
          "Application",
          {std::make_pair("", "Test"), std::make_pair("fr", "Test French")},
          {std::make_pair("", "Test me out!"),
           std::make_pair("es", "Hola for the comment")},
          true,
          "prettyicon",
          true,
          {"KDE", "Gnome"},
          "mybinary",
          "mybinary %F",
          "/usr/local/bin",
          true,
          {"text/plain", "foo/x-java"},
          {"Magic", "Playtime"},
          "classy",
      },
      true);
}

TEST_F(DesktopFileTest, Locales) {
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name=LocaleTest\n"
      "Name[sr]=Test sr foo\n"
      "Name[sr_YU]=Test sr underscore YU foo\n"
      "Name[sr_YU@Latn]=Test sr underscore YU at Latn foo\n"
      "Name[sr@Latn]=Test sr at Latn foo\n"
      "Name[ab]=Test ab foo\n"
      "Name[ab_cd]=Test ab underscore cd foo\n"
      "Name[ab_cd@xyz]=Test ab underscore cd at xyz foo\n"
      "Name[ab@xyz]=Test ab at xyz foo\n",
      "locales.desktop",
      {
          "locales",
          "Application",
          {
              std::make_pair("", "LocaleTest"),
              std::make_pair("sr", "Test sr foo"),
              std::make_pair("sr_YU", "Test sr underscore YU foo"),
              std::make_pair("sr_YU@Latn", "Test sr underscore YU at Latn foo"),
              std::make_pair("sr@Latn", "Test sr at Latn foo"),
              std::make_pair("ab", "Test ab foo"),
              std::make_pair("ab_cd", "Test ab underscore cd foo"),
              std::make_pair("ab_cd@xyz", "Test ab underscore cd at xyz foo"),
              std::make_pair("ab@xyz", "Test ab at xyz foo"),
          },
      },
      true);
}

TEST_F(DesktopFileTest, Escaping) {
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name=Test \\\"Quoted\\\" \\t tab \\s space \\r CR \\n newline \\\\ "
      "backslash\n"
      "OnlyShowIn=semicolon\\;;;AfterEmpty;Another\\;Semi;\n",
      "EscapeMe.desktop",
      {
          "EscapeMe",
          "Application",
          {
              std::make_pair("",
                             "Test \"Quoted\" \t tab   space \r CR \n newline "
                             "\\ backslash"),
          },
          {},
          false,
          "",
          false,
          {"semicolon;", "", "AfterEmpty", "Another;Semi"},
      },
      true);
}

TEST_F(DesktopFileTest, WhitespaceRemoval) {
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type =Application \n"
      "Name = TestW\n",
      "whitespace.desktop",
      {
          "whitespace", "Application", {std::make_pair("", "TestW")},
      },
      true);
}

TEST_F(DesktopFileTest, Types) {
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name=TestApplication\n",
      "ApplicationTest.desktop",
      {
          "ApplicationTest",
          "Application",
          {std::make_pair("", "TestApplication")},
      },
      true);
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type=Directory\n"
      "Name=TestDirectory\n",
      "DirectoryTest.desktop",
      {
          "DirectoryTest", "Directory", {std::make_pair("", "TestDirectory")},
      },
      true);
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type=Link\n"
      "Name=TestLink\n",
      "LinkTest.desktop",
      {
          "LinkTest", "Link", {std::make_pair("", "TestLink")},
      },
      true);
  // Now try an invalid type, which should fail
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type=FakeType\n"
      "Name=TestLink\n",
      "faketype.desktop", {}, false);
}

TEST_F(DesktopFileTest, RelativePathConversion) {
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name=Test\n",
      "foo/bar_fun/mad.desktop",
      {
          "foo-bar_fun-mad", "Application", {std::make_pair("", "Test")},
      },
      true);
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name=Test\n",
      "foo/applications/bar.desktop",
      {
          "foo-applications-bar", "Application", {std::make_pair("", "Test")},
      },
      true);
}

TEST_F(DesktopFileTest, IgnoreOtherGroups) {
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name=TestApplication\n"
      "[Desktop Action Foo]\n"
      "Type=Directory\n"
      "Name=BadApplication\n",
      "ApplicationTest.desktop",
      {
          "ApplicationTest",
          "Application",
          {std::make_pair("", "TestApplication")},
      },
      true);
}

TEST_F(DesktopFileTest, MissingNameFails) {
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name[fr]=AlsoNeedNoLocaleName\n",
      "MissingName.desktop", {}, false);
}

TEST_F(DesktopFileTest, InvalidFileExtensionFails) {
  ValidateDesktopFile(
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Name=TestName\n",
      "badextension.notdesktop", {}, false);
}

}  // namespace garcon
}  // namespace vm_tools
