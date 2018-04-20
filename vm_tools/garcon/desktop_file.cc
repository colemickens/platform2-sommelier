// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <memory>
#include <utility>

#include <base/environment.h>
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
constexpr char kDesktopEntryStartupNotify[] = "StartupNotify";
constexpr char kDesktopEntryTypeApplication[] = "Application";
// Valid values for the "Type" entry.
const char* const kValidDesktopEntryTypes[] = {kDesktopEntryTypeApplication,
                                               "Link", "Directory"};
constexpr char kXdgDataDirsEnvVar[] = "XDG_DATA_DIRS";
// Default path to to use if the XDG_DATA_DIRS env var is not set.
constexpr char kDefaultDesktopFilesPath[] = "/usr/share";
constexpr char kSettingsCategory[] = "Settings";
constexpr char kPathEnvVar[] = "PATH";

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

// static
std::vector<base::FilePath> DesktopFile::GetPathsForDesktopFiles() {
  const char* xdg_data_dirs = getenv(kXdgDataDirsEnvVar);
  if (!xdg_data_dirs || !strlen(xdg_data_dirs)) {
    xdg_data_dirs = kDefaultDesktopFilesPath;
  }
  // Now break it up into the paths that we should search.
  std::vector<base::StringPiece> search_dirs = base::SplitStringPiece(
      xdg_data_dirs, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<base::FilePath> retval;
  for (const auto& curr_dir : search_dirs) {
    base::FilePath curr_path(curr_dir);
    retval.emplace_back(curr_path.Append(kDesktopPathStartDelimiter));
  }
  return retval;
}

// static
base::FilePath DesktopFile::FindFileForDesktopId(
    const std::string& desktop_id) {
  if (desktop_id.empty()) {
    return base::FilePath();
  }
  // First we need to create the relative path that corresponds to this ID. This
  // is done by replacing all dash chars with the path separator and then
  // appending the .desktop file extension to the name. Alternatively, we also
  // look without doing any replacing.
  std::string rel_path1;
  base::ReplaceChars(desktop_id, "-", "/", &rel_path1);
  rel_path1 += kDesktopFileExtension;
  std::string rel_paths[] = {rel_path1, desktop_id + kDesktopFileExtension};

  std::vector<base::FilePath> search_paths = GetPathsForDesktopFiles();
  for (const auto& curr_path : search_paths) {
    for (const auto& rel_path : rel_paths) {
      base::FilePath test_path = curr_path.Append(rel_path);
      if (base::PathExists(test_path)) {
        return test_path;
      }
    }
  }
  return base::FilePath();
}

bool DesktopFile::LoadFromFile(const base::FilePath& file_path) {
  // First read in the file as a string.
  std::string desktop_contents;
  if (!ReadFileToStringWithMaxSize(file_path, &desktop_contents,
                                   kMaxDesktopFileSize)) {
    LOG(ERROR) << "Failed reading in desktop file: " << file_path.value();
    return false;
  }
  file_path_ = file_path;

  std::vector<base::StringPiece> desktop_lines = base::SplitStringPiece(
      desktop_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Go through the file line by line, we are looking for the section marked:
  // [Desktop Entry]
  bool in_entry = false;
  for (const auto& curr_line : desktop_lines) {
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
      } else if (key == kDesktopEntryStartupNotify) {
        startup_notify_ = ParseBool(key_value.second);
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

std::vector<std::string> DesktopFile::GenerateArgvWithFiles(
    const std::vector<std::string>& app_args) const {
  std::vector<std::string> retval;
  if (exec_.empty()) {
    return retval;
  }
  // We have already unescaped this string, which we are supposed to do first
  // according to the spec. We need to process this to handle quoted arguments
  // and also field code substitution.
  std::string curr_arg;
  bool in_quotes = false;
  bool next_escaped = false;
  bool next_field_code = false;
  for (auto c : exec_) {
    if (next_escaped) {
      next_escaped = false;
      curr_arg.push_back(c);
      continue;
    }
    if (c == '"') {
      if (in_quotes && !curr_arg.empty()) {
        // End of a quoted argument.
        retval.emplace_back(std::move(curr_arg));
        curr_arg.clear();
      }
      in_quotes = !in_quotes;
      continue;
    }
    if (in_quotes) {
      // There is no field expansion inside quotes, so just append the char
      // unless we have escaping. We only deal with escaping inside of quoted
      // strings here.
      if (c == '\\') {
        next_escaped = true;
        continue;
      }
      curr_arg.push_back(c);
      continue;
    }
    if (next_field_code) {
      next_field_code = false;
      if (c == '%') {
        // Escaped percent sign (I don't know why they just didn't use backslash
        // for escaping percent).
        curr_arg.push_back(c);
        continue;
      }
      switch (c) {
        case 'u':  // Single URL field code.
        case 'f':  // Single file field code.
          if (!app_args.empty()) {
            curr_arg.append(app_args.front());
          }
          continue;
        case 'U':  // Multiple URLs field code.
        case 'F':  // Multiple files field code.
          // For multi-args, the spec is explicit that each file is passed as
          // a separate arg to the program and that %U and %F must only be
          // used as an argument on their own, so complete any active arg
          // that we may have been parsing.
          if (!curr_arg.empty()) {
            retval.emplace_back(std::move(curr_arg));
            curr_arg.clear();
          }
          if (!app_args.empty()) {
            retval.insert(retval.end(), app_args.begin(), app_args.end());
          }
          continue;
        case 'i':  // Icon field code, expands to 2 args.
          if (!curr_arg.empty()) {
            retval.emplace_back(std::move(curr_arg));
            curr_arg.clear();
          }
          if (!icon_.empty()) {
            retval.emplace_back("--icon");
            retval.emplace_back(icon_);
          }
          continue;
        case 'c':  // Translated app name.
          // TODO(jkardatzke): Determine the proper localized name for the app.
          // We enforce that this key exists when we populate the object.
          curr_arg.append(locale_name_map_.find("")->second);
          continue;
        case 'k':  // Path to the desktop file itself.
          curr_arg.append(file_path_.value());
          continue;
        default:  // Unrecognized/deprecated field code. Unrecognized ones are
                  // technically invalid, but it seems better to just ignore
                  // them then completely abort executing this desktop file.
          continue;
      }
    }
    if (c == ' ') {
      // Argument separator.
      if (!curr_arg.empty()) {
        retval.emplace_back(std::move(curr_arg));
        curr_arg.clear();
      }
      continue;
    }
    if (c == '%') {
      next_field_code = true;
      continue;
    }
    curr_arg.push_back(c);
  }
  if (!curr_arg.empty()) {
    retval.emplace_back(std::move(curr_arg));
  }
  return retval;
}

bool DesktopFile::ShouldPassToHost() const {
  // Rules to follow:
  // -Only allow Applications.
  // -Don't pass hidden.
  // -Don't pass without an exec entry.
  // -Don't pass no_display that also have no mime types.
  // -Don't pass where OnlyShowIn has entries.
  // -Don't pass terminal apps (e.g. apps that run in a terminal like vim).
  // -Don't pass if in the Settings category.
  // -Don't pass if TryExec doesn't resolve to a valid executable file.
  if (!IsApplication() || hidden_ || exec_.empty() ||
      (no_display_ && mime_types_.empty()) || !only_show_in_.empty() ||
      terminal_) {
    return false;
  }

  for (const auto& category : categories_) {
    if (category == kSettingsCategory) {
      return false;
    }
  }

  if (!try_exec_.empty()) {
    // If it's absolute, we just check it the way it is.
    base::FilePath try_exec_path(try_exec_);
    if (try_exec_path.IsAbsolute()) {
      int permissions;
      if (!base::GetPosixFilePermissions(try_exec_path, &permissions) ||
          !(permissions & base::FILE_PERMISSION_EXECUTE_BY_USER)) {
        return false;
      }
    } else {
      // Search the system path instead.
      std::string path;
      if (!base::Environment::Create()->GetVar(kPathEnvVar, &path)) {
        // If there's no PATH set we can't search.
        return false;
      }
      bool found_match = false;
      for (const base::StringPiece& cur_path : base::SplitStringPiece(
               path, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
        base::FilePath file(cur_path);
        int permissions;
        if (base::GetPosixFilePermissions(file.Append(try_exec_),
                                          &permissions) &&
            (permissions & base::FILE_PERMISSION_EXECUTE_BY_USER)) {
          found_match = true;
          break;
        }
      }
      if (!found_match) {
        return false;
      }
    }
  }

  return true;
}

bool DesktopFile::IsApplication() const {
  return entry_type_ == kDesktopEntryTypeApplication;
}

}  // namespace garcon
}  // namespace vm_tools
