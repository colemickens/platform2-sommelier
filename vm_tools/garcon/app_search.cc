// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/garcon/app_search.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_split.h>
#include <base/strings/string_tokenizer.h>
#include <base/strings/string_util.h>

#include <algorithm>

namespace {
constexpr char kDevelFacetPrefix[] = "devel::";
constexpr char kGraphicalTag[] = "interface::graphical";
// TODO(danielng): Need UX spec.
constexpr double ResultThreshold = 0.6;
bool OrderBySecondDescending(const std::pair<std::string, float>& x,
                             const std::pair<std::string, float>& y) {
  if (x.second == y.second) {
    return x.first < y.first;
  } else {
    return x.second > y.second;
  }
}
}  // namespace

namespace vm_tools {
namespace garcon {

bool ParseDebtags(const std::string& file_name,
                  std::vector<std::string>* out_packages,
                  std::string* out_error) {
  CHECK(out_packages);
  CHECK(out_error);
  base::FilePath file_path(file_name);
  if (!base::PathExists(file_path)) {
    out_error->assign("package-tags file '" + file_name + "' does not exist");
    LOG(ERROR) << *out_error;
    return false;
  }

  std::string file_contents;
  if (!base::ReadFileToString(file_path, &file_contents)) {
    out_error->assign("Failed reading in package-tags file '" + file_name +
                      "'");
    PLOG(ERROR) << *out_error;
    return false;
  }

  // See: https://sources.debian.org/src/debtags/2.1.5/debtags/#L601
  std::vector<base::StringPiece> lines = base::SplitStringPiece(
      file_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& curr_line : lines) {
    size_t pos = curr_line.find_first_of(":", 0);
    base::StringPiece name = curr_line.substr(0, pos);

    std::vector<base::StringPiece> tags = base::SplitStringPiece(
        curr_line.substr(pos + 1), ",", base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);

    // Track what tags a package has.
    bool contains_devel = false;
    bool contains_graphical = false;
    for (const auto& tag : tags) {
      if (!contains_devel && tag.starts_with(kDevelFacetPrefix))
        contains_devel = true;

      if (!contains_graphical && tag == kGraphicalTag)
        contains_graphical = true;

      if (contains_devel && contains_graphical) {
        out_packages->push_back(name.as_string());
        break;
      }
    }
  }
  return true;
}

std::vector<std::pair<std::string, float>> SearchPackages(
    const std::vector<std::string>& package_list, const std::string& query) {
  std::vector<std::pair<std::string, float>> results;
  std::string query_lower_case = base::ToLowerASCII(query);
  for (auto& package_name : package_list) {
    if (package_name.find(query_lower_case) != base::StringPiece::npos) {
      // TODO(danielng): expand logic for ranking search results,
      // possibly look to incorporating popularity statistics
      double score =
          static_cast<float>(query_lower_case.size()) / package_name.size();
      if (score >= ResultThreshold)
        results.emplace_back(package_name, score);
    }
  }
  std::sort(results.begin(), results.end(), OrderBySecondDescending);
  return results;
}

}  // namespace garcon
}  // namespace vm_tools
