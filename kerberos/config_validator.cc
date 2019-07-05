// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/config_validator.h"

#include <vector>

#include <base/stl_util.h>
#include <base/strings/string_split.h>

namespace kerberos {
namespace {

// See
// https://web.mit.edu/kerberos/krb5-1.12/doc/admin/conf_files/krb5_conf.html
// for a description of the krb5.conf format.

// Directives that are not relations (i.e. key=value). All blacklisted.
const char* const kDirectives[] = {"module", "include", "includedir"};

// Whitelisted configuration keys in the [libdefaults] section.
const char* const kLibDefaultsWhitelist[] = {
    "canonicalize",
    "clockskew",
    "default_tgs_enctypes",
    "default_tkt_enctypes",
    "dns_canonicalize_hostname",
    "dns_lookup_kdc",
    "extra_addresses",
    "forwardable",
    "ignore_acceptor_hostname",
    "kdc_default_options",
    "kdc_timesync",
    "noaddresses",
    "permitted_enctypes",
    "preferred_preauth_types",
    "proxiable",
    "rdns",
    "renew_lifetime",
    "ticket_lifetime",
    "udp_preference_limit",
};

// Whitelisted configuration keys in the [realms] section.
const char* const kRealmsWhitelist[] = {
    "admin_server", "auth_to_local", "kdc", "kpasswd_server", "master_kdc",
};

// Whitelisted sections. Any key in "domain_realm" and "capaths" is accepted.
constexpr char kSectionLibdefaults[] = "libdefaults";
constexpr char kSectionRealms[] = "realms";
constexpr char kSectionDomainRealm[] = "domain_realm";
constexpr char kSectionCapaths[] = "capaths";

const char* const kSectionWhitelist[] = {kSectionLibdefaults, kSectionRealms,
                                         kSectionDomainRealm, kSectionCapaths};

ConfigErrorInfo MakeErrorInfo(ConfigErrorCode code, int line_index) {
  ConfigErrorInfo error_info;
  error_info.set_code(code);
  error_info.set_line_index(line_index);
  return error_info;
}

}  // namespace

ConfigValidator::ConfigValidator()
    : libdefaults_whitelist_(std::begin(kLibDefaultsWhitelist),
                             std::end(kLibDefaultsWhitelist)),
      realms_whitelist_(std::begin(kRealmsWhitelist),
                        std::end(kRealmsWhitelist)),
      section_whitelist_(std::begin(kSectionWhitelist),
                         std::end(kSectionWhitelist)) {}

ConfigErrorInfo ConfigValidator::Validate(const std::string& krb5conf) const {
  // Keep empty lines, they're necessary to get the line numbers right.
  const std::vector<std::string> lines = base::SplitString(
      krb5conf, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Level of nested curly braces {}.
  int group_level = 0;

  // Opening curly braces '{' can be on the same line and on the next line. This
  // is set to true if a '{' is expected on the next line.
  bool expect_opening_curly_brace = false;

  // Current [section].
  std::string current_section;

  for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
    const std::string& line = lines.at(line_index);

    // Are we expecting a '{' to open a { group }?
    if (expect_opening_curly_brace) {
      if (line.empty() || line.at(0) != '{') {
        return MakeErrorInfo(CONFIG_ERROR_EXPECTED_OPENING_CURLY_BRACE,
                             line_index);
      }
      group_level++;
      expect_opening_curly_brace = false;
      continue;
    }

    // Skip empty lines.
    if (line.empty())
      continue;

    // Skip comments.
    if (line.at(0) == ';' || line.at(0) == '#')
      continue;

    // Bail on any |kDirectives|.
    for (const char* directive : kDirectives) {
      const int len = strlen(directive);
      if (strncmp(line.c_str(), directive, len) == 0 && isspace(line.at(len)))
        return MakeErrorInfo(CONFIG_ERROR_KEY_NOT_SUPPORTED, line_index);
    }

    // Check for '}' to close a { group }.
    if (line.at(0) == '}') {
      if (group_level == 0)
        return MakeErrorInfo(CONFIG_ERROR_EXTRA_CURLY_BRACE, line_index);
      group_level--;
      continue;
    }

    // Check for new [section].
    if (line.at(0) == '[') {
      // Bail if section is within a { group }.
      if (group_level > 0)
        return MakeErrorInfo(CONFIG_ERROR_SECTION_NESTED_IN_GROUP, line_index);

      // Bail if closing bracket is missing or if there's more stuff after the
      // closing bracket (the final marker '*' is fine).
      std::vector<std::string> parts = base::SplitString(
          line, "]", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      if (parts.size() != 2 || !(parts.at(1).empty() || parts.at(1) == "*"))
        return MakeErrorInfo(CONFIG_ERROR_SECTION_SYNTAX, line_index);

      current_section = parts.at(0).substr(1);

      // Bail if the section is not supported, e.g. [appdefaults].
      if (current_section.empty() ||
          !base::ContainsKey(section_whitelist_, current_section.c_str())) {
        return MakeErrorInfo(CONFIG_ERROR_SECTION_NOT_SUPPORTED, line_index);
      }
      continue;
    }

    // Check for "key = value" or "key = {".
    std::vector<std::string> parts = base::SplitString(
        line, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    // Remove final marker.
    std::string& key = parts.at(0);
    if (key.back() == '*')
      key.pop_back();

    // Final marker must come immediately after key.
    if (key.empty() || isspace(key.back()))
      return MakeErrorInfo(CONFIG_ERROR_RELATION_SYNTAX, line_index);

    // Is there at least one '=' sign?
    if (parts.size() < 2)
      return MakeErrorInfo(CONFIG_ERROR_RELATION_SYNTAX, line_index);

    // Check for a '{' to start a group. The '{' could also be on the next line.
    // If there's anything except whitespace after '{', it counts as value, not
    // as a group.
    const std::string& value = parts.at(1);
    if (value.empty()) {
      expect_opening_curly_brace = true;
      continue;
    }
    if (value == "{" && parts.size() == 2) {
      group_level++;
      continue;
    }

    // Check whether we support the key.
    if (!IsKeySupported(key, current_section, group_level))
      return MakeErrorInfo(CONFIG_ERROR_KEY_NOT_SUPPORTED, line_index);
  }

  ConfigErrorInfo error_info;
  error_info.set_code(CONFIG_ERROR_NONE);
  return error_info;
}

bool ConfigValidator::IsKeySupported(const std::string& key,
                                     const std::string& section,
                                     int group_level) const {
  // Bail on anything outside of a section.
  if (section.empty())
    return false;

  // Enforce only whitelisted libdefaults keys on the root and realm levels:
  // [libdefaults]
  //   clockskew = 300
  //   EXAMPLE.COM = {
  //     clockskew = 500
  //   }
  if (section == kSectionLibdefaults && group_level <= 1) {
    return base::ContainsKey(libdefaults_whitelist_, key.c_str());
  }

  // Enforce only whitelisted realm keys on the root and realm levels:
  // [realms]
  //   kdc = kerberos1.example.com
  //   EXAMPLE.COM = {
  //      kdc = kerberos2.example.com
  //   }
  // Not sure if they can actually be at the root level, but just in case...
  if (section == kSectionRealms && group_level <= 1)
    return base::ContainsKey(realms_whitelist_, key.c_str());

  // Anything else is fine (all keys of other supported sections).
  return true;
}

bool ConfigValidator::StrEquals::operator()(const char* a,
                                            const char* b) const {
  return strcmp(a, b) == 0;
}

size_t ConfigValidator::StrHash::operator()(const char* str) const {
  // Taken from base::StringPieceHash.
  std::size_t result = 0;
  for (const char* c = str; *c; ++c)
    result = (result * 131) + *c;
  return result;
}

}  // namespace kerberos
