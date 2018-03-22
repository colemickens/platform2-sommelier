// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

#include "vm_tools/garcon/desktop_file.h"

namespace {
// Ridiculously large size for a desktop file.
constexpr size_t kMaxDesktopFileSize = 10485760;  // 10 MB
// Group name for the main entry we want.
constexpr char kDesktopEntryGroupName[] = "Desktop Entry";
// File extension for desktop files.
constexpr char kDesktopFileExtension[] = ".desktop";
// Desktop path start delimiter for constructing application IDs.
constexpr char kDesktopPathStartDelimiter[] = "applications";
// Key names for the fields we care about.
constexpr char kDesktopEntryType[] = "Type";
constexpr char kDesktopEntryName[] = "Name";
constexpr char kDesktopEntryNameWithLocale[] = "Name[";
constexpr char kDesktopEntryNoDisplay[] = "NoDisplay";
constexpr char kDesktopEntryComment[] = "Comment";
constexpr char kDesktopEntryCommentWithLocale[] = "Comment[";
constexpr char kDesktopEntryIcon[] = "Icon";
constexpr char kDesktopEntryHidden[] = "Hidden";
constexpr char kDesktopEntryOnlyShowIn[] = "OnlyShowIn";
constexpr char kDesktopEntryTryExec[] = "TryExec";
constexpr char kDesktopEntryExec[] = "Exec";
constexpr char kDesktopEntryPath[] = "Path";
constexpr char kDesktopEntryTerminal[] = "Terminal";
constexpr char kDesktopEntryMimeType[] = "MimeType";
constexpr char kDesktopEntryCategories[] = "Categories";
constexpr char kDesktopEntryStartupWmClass[] = "StartupWMClass";
// Valid values for the "Type" entry.
const char* kValidDesktopEntryTypes[] = {"Application", "Link", "Directory"};

// Extracts name from "[Name]" formatted string, empty string returned if error.
base::StringPiece ParseGroupName(base::StringPiece group_line) {
  if (group_line.empty() || group_line.front() != '[' ||
      group_line.back() != ']') {
    return base::StringPiece();
  }
  return group_line.substr(1, group_line.size() - 2);
}

// Converts a boolean string value to primitive.
bool ParseBool(const std::string& s) {
  return s == "true";
}

// Gets the locale value out of a key name, which is in the format:
// "key[locale]". Returns empty string if this had an invalid format.
std::string ExtractKeyLocale(const std::string& key) {
  if (key.back() != ']') {
    return "";
  }
  size_t bracket_pos = key.find_first_of('[');
  if (bracket_pos == std::string::npos) {
    return "";
  }
  return key.substr(bracket_pos + 1, key.length() - bracket_pos - 2);
}

// Returns a std::pair of strings that is extracted from the passed in string.
// This uses '=' as the delimiter between the key and value pair. Any whitespace
// around the '=' is removed. If there is no delimiter, then the second item in
// the pair will be empty.
std::pair<std::string, std::string> ExtractKeyValuePair(
    base::StringPiece entry_line) {
  size_t equal_pos = entry_line.find_first_of('=');
  if (equal_pos == std::string::npos) {
    return std::make_pair(entry_line.as_string(), "");
  }
  base::StringPiece key = base::TrimWhitespaceASCII(
      entry_line.substr(0, equal_pos), base::TRIM_TRAILING);
  base::StringPiece value = base::TrimWhitespaceASCII(
      entry_line.substr(equal_pos + 1), base::TRIM_LEADING);
  return std::make_pair(key.as_string(), value.as_string());
}

// Converts all escaped chars in this string to their proper equivalent.
std::string UnescapeString(const std::string& s) {
  std::string retval;
  bool is_escaped = false;
  for (auto c : s) {
    if (is_escaped) {
      switch (c) {
        case 's':
          retval.push_back(' ');
          break;
        case 't':
          retval.push_back('\t');
          break;
        case 'r':
          retval.push_back('\r');
          break;
        case 'n':
          retval.push_back('\n');
          break;
        default:
          retval.push_back(c);
          break;
      }
      is_escaped = false;
      continue;
    }
    if (c == '\\') {
      is_escaped = true;
      continue;
    }
    retval.push_back(c);
  }
  return retval;
}

// Parses the passed in string into parts that are delimited by semicolon. This
// also allows escaping of semicolons with the backslash character which is why
// we can't use standard string splitting.
void ParseMultiString(const std::string& s, std::vector<std::string>* out) {
  CHECK(out);
  std::string curr;
  bool use_next = false;
  for (auto c : s) {
    if (use_next) {
      use_next = false;
      curr.push_back(c);
      continue;
    }
    if (c == ';') {
      out->emplace_back(UnescapeString(curr));
      curr.clear();
      continue;
    }
    if (c == '\\') {
      // Leave the backslashes in there since we will be unescaping this string
      // still.
      use_next = true;
    }
    curr.push_back(c);
  }
  if (!curr.empty()) {
    out->emplace_back(UnescapeString(curr));
  }
}

}  // namespace

namespace vm_tools {
namespace garcon {

// static
std::unique_ptr<DesktopFile> DesktopFile::ParseDesktopFile(
    const base::FilePath& file_path) {
  std::unique_ptr<DesktopFile> retval(new DesktopFile());
  if (!retval->LoadFromFile(file_path)) {
    retval.reset();
  }
  return retval;
}

bool DesktopFile::LoadFromFile(const base::FilePath& file_path) {
  // First read in the file as a string.
  std::string desktop_contents;
  if (!ReadFileToStringWithMaxSize(file_path, &desktop_contents,
                                   kMaxDesktopFileSize)) {
    LOG(ERROR) << "Failed reading in desktop file: " << file_path.value();
    return false;
  }

  std::vector<base::StringPiece> desktop_lines = base::SplitStringPiece(
      desktop_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Go through the file line by line, we are looking for the section marked:
  // [Desktop Entry]
  bool in_entry = false;
  for (auto curr_line : desktop_lines) {
    if (curr_line.front() == '#') {
      // Skip comment lines.
      continue;
    }
    if (curr_line.front() == '[') {
      if (in_entry) {
        // We only care about the main entry, so terminate parsing if we have
        // found it.
        break;
      }
      // Group name.
      base::StringPiece group_name = ParseGroupName(curr_line);
      if (group_name.empty()) {
        continue;
      }
      if (group_name == kDesktopEntryGroupName) {
        in_entry = true;
      }
    } else if (!in_entry) {
      // We are not in the main entry, and this line doesn't begin that entry so
      // skip it.
      continue;
    } else {
      // Parse the key/value pair on this line for the desktop entry.
      std::pair<std::string, std::string> key_value =
          ExtractKeyValuePair(curr_line);
      if (key_value.second.empty()) {
        // Invalid key/value pair since there was no delimiter, skip ths line.
        continue;
      }
      // Check for matching names against all the keys. For the ones that can
      // have a locale in the key name, do those last since we do a startsWith
      // comparison on those.
      std::string key = key_value.first;
      if (key == kDesktopEntryType) {
        entry_type_ = key_value.second;
      } else if (key == kDesktopEntryName) {
        locale_name_map_[""] = UnescapeString(key_value.second);
      } else if (key == kDesktopEntryNoDisplay) {
        no_display_ = ParseBool(key_value.second);
      } else if (key == kDesktopEntryComment) {
        locale_comment_map_[""] = UnescapeString(key_value.second);
      } else if (key == kDesktopEntryIcon) {
        icon_ = key_value.second;
      } else if (key == kDesktopEntryHidden) {
        hidden_ = ParseBool(key_value.second);
      } else if (key == kDesktopEntryOnlyShowIn) {
        ParseMultiString(key_value.second, &only_show_in_);
      } else if (key == kDesktopEntryTryExec) {
        try_exec_ = UnescapeString(key_value.second);
      } else if (key == kDesktopEntryExec) {
        exec_ = UnescapeString(key_value.second);
      } else if (key == kDesktopEntryPath) {
        path_ = UnescapeString(key_value.second);
      } else if (key == kDesktopEntryTerminal) {
        terminal_ = ParseBool(key_value.second);
      } else if (key == kDesktopEntryMimeType) {
        ParseMultiString(key_value.second, &mime_types_);
      } else if (key == kDesktopEntryCategories) {
        ParseMultiString(key_value.second, &categories_);
      } else if (key == kDesktopEntryStartupWmClass) {
        startup_wm_class_ = UnescapeString(key_value.second);
      } else if (base::StartsWith(key, kDesktopEntryNameWithLocale,
                                  base::CompareCase::SENSITIVE)) {
        std::string locale = ExtractKeyLocale(key);
        if (locale.empty()) {
          continue;
        }
        locale_name_map_[locale] = UnescapeString(key_value.second);
      } else if (base::StartsWith(key, kDesktopEntryCommentWithLocale,
                                  base::CompareCase::SENSITIVE)) {
        std::string locale = ExtractKeyLocale(key);
        if (locale.empty()) {
          continue;
        }
        locale_comment_map_[locale] = UnescapeString(key_value.second);
      }
    }
  }

  // Validate that the desktop file has the required entries in it.
  // First check the Type key.
  bool valid_type_found = false;
  for (const char* valid_type : kValidDesktopEntryTypes) {
    if (entry_type_ == valid_type) {
      valid_type_found = true;
      break;
    }
  }
  if (!valid_type_found) {
    LOG(ERROR) << "Failed parsing desktop file " << file_path.value()
               << " due to invalid Type key of: " << entry_type_;
    return false;
  }
  // Now check for a valid name.
  if (locale_name_map_.find("") == locale_name_map_.end()) {
    LOG(ERROR) << "Failed parsing desktop file " << file_path.value()
               << " due to missing unlocalized Name entry";
    return false;
  }
  // Since it's valid, set the ID based on the path name. This is done by
  // taking all the path values after "applications" in the path, appending them
  // with dash separators and then removing the .desktop extension from the
  // actual filename.
  // First verify this was actually a .desktop file.
  if (file_path.FinalExtension() != kDesktopFileExtension) {
    LOG(ERROR) << "Failed parsing desktop file due to invalid file extension: "
               << file_path.value();
    return false;
  }
  std::vector<base::FilePath::StringType> path_comps;
  file_path.RemoveFinalExtension().GetComponents(&path_comps);
  bool found_path_delim = false;
  for (const auto& comp : path_comps) {
    if (!found_path_delim) {
      found_path_delim = (comp == kDesktopPathStartDelimiter);
      continue;
    }
    if (!app_id_.empty()) {
      app_id_.push_back('-');
    }
    app_id_.append(comp);
  }

  return true;
}

}  // namespace garcon
}  // namespace vm_tools
