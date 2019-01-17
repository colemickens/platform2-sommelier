// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_GARCON_APP_SEARCH_H_
#define VM_TOOLS_GARCON_APP_SEARCH_H_

#include <string>
#include <utility>
#include <vector>

namespace vm_tools {
namespace garcon {

// Parses the package-tags file (handled by the debtags package) and filters
// out any packages that do not contain the "devel" debtag and either
// the "interface::graphical" or the "suite" debtags. Packages not filtered out
// are stored in the |out_packages| vector.
bool ParseDebtags(const std::string& file_name,
                  std::vector<std::string>* out_packages,
                  std::string* out_error);

// Searches the package list for the passed plaintext search query and returns
// a vector of pairs of package names that match the query with their relevance
// score [0,1].
std::vector<std::pair<std::string, float>> SearchPackages(
    const std::vector<std::string>& package_list, const std::string& query);

}  // namespace garcon
}  // namespace vm_tools

#endif  // VM_TOOLS_GARCON_APP_SEARCH_H_
