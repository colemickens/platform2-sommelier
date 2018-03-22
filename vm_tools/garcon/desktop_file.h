// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_GARCON_DESKTOP_FILE_H_
#define VM_TOOLS_GARCON_DESKTOP_FILE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>

namespace vm_tools {
namespace garcon {
// Parses .desktop files according to the Desktop Entry Specification here:
// https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-1.2.html
class DesktopFile {
 public:
  // Returns empty unique_ptr if there was a failure parsing the .desktop file.
  static std::unique_ptr<DesktopFile> ParseDesktopFile(
      const base::FilePath& file_path);
  ~DesktopFile() = default;

  const std::string& app_id() const { return app_id_; }
  const std::string& entry_type() const { return entry_type_; }
  const std::map<std::string, std::string>& locale_name_map() const {
    return locale_name_map_;
  }
  const std::map<std::string, std::string>& locale_comment_map() const {
    return locale_comment_map_;
  }
  bool no_display() const { return no_display_; }
  const std::string& icon() { return icon_; }
  bool hidden() const { return hidden_; }
  const std::vector<std::string>& only_show_in() const { return only_show_in_; }
  const std::string& try_exec() const { return try_exec_; }
  const std::string& exec() const { return exec_; }
  const std::string& path() const { return path_; }
  bool terminal() const { return terminal_; }
  const std::vector<std::string>& mime_types() const { return mime_types_; }
  const std::vector<std::string>& categories() const { return categories_; }
  const std::string& startup_wm_class() const { return startup_wm_class_; }

 private:
  DesktopFile() = default;
  bool LoadFromFile(const base::FilePath& file_path);

  std::string app_id_;
  std::string entry_type_;
  std::map<std::string, std::string> locale_name_map_;
  std::map<std::string, std::string> locale_comment_map_;
  bool no_display_ = false;
  std::string icon_;
  bool hidden_ = false;
  std::vector<std::string> only_show_in_;
  std::string try_exec_;
  std::string exec_;
  std::string path_;
  bool terminal_ = false;
  std::vector<std::string> mime_types_;
  std::vector<std::string> categories_;
  std::string startup_wm_class_;

  DISALLOW_COPY_AND_ASSIGN(DesktopFile);
};

}  // namespace garcon
}  // namespace vm_tools

#endif  // VM_TOOLS_GARCON_DESKTOP_FILE_H_
