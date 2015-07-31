// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromiumos-wide-profiling/perf_option_parser.h"

#include <map>

#include "chromiumos-wide-profiling/compat/string.h"

namespace quipper {

namespace {

enum class OptionType {
  Boolean,  // has no value
  Value,    // Uses another argument.
};

const std::map<string, OptionType>& GetPerfRecordOptions() {
  static const auto* kPerfRecordOptions = new std::map<string, OptionType> {
    {"-e", OptionType::Value},
    {"--event", OptionType::Value},
    {"--filter", OptionType::Value},
    {"-p", OptionType::Value},
    {"--pid", OptionType::Value},
    {"-t", OptionType::Value},
    {"--tid", OptionType::Value},
    {"-r", OptionType::Value},
    {"--realtime", OptionType::Value},
    /* Banned: {"--no-buffering", OptionType::Boolean}, */
    {"-R", OptionType::Boolean},
    {"--raw-samples", OptionType::Boolean},
    {"-a", OptionType::Boolean},
    {"--all-cpus", OptionType::Boolean},
    {"-C", OptionType::Value},
    {"--cpu", OptionType::Value},
    {"-c", OptionType::Value},
    {"--count", OptionType::Value},
    /* Banned: {"-o", OptionType::Value},
     * {"--output", OptionType::Value}, */
    {"-i", OptionType::Boolean},
    {"--no-inherit", OptionType::Boolean},
    {"-F", OptionType::Value},
    {"--freq", OptionType::Value},
    /* Banned: {"-m", OptionType::Value},
     * {"--mmap-pages", OptionType::Value}, */
    {"--group", OptionType::Boolean},  /* new? */
    {"-g", OptionType::Boolean},  /* NB: in stat, this is short for --group */
    {"--call-graph", OptionType::Value},
    /* Banned: {"-v", OptionType::Boolean},
     * {"--verbose", OptionType::Boolean}, */
    /* Banned: {"-q", OptionType::Boolean},
     * {"--quiet", OptionType::Boolean}, */
    {"-s", OptionType::Boolean},
    {"--stat", OptionType::Boolean},
    {"-d", OptionType::Boolean},
    {"--data", OptionType::Boolean},
    {"-T", OptionType::Boolean},
    {"--timestamp", OptionType::Boolean},
    {"-P", OptionType::Boolean},  /* new? */
    {"--period", OptionType::Boolean}, /* new? */
    {"-n", OptionType::Boolean},
    {"--no-samples", OptionType::Boolean},
    {"-N", OptionType::Boolean},
    {"--no-buildid-cache", OptionType::Boolean},
    {"-B", OptionType::Boolean},  /* new? */
    {"--no-buildid", OptionType::Boolean},  /* new? */
    {"-G", OptionType::Value},
    {"--cgroup", OptionType::Value},
    /* Changed between v3.13 to v3.14 from:
    {"-D", OptionType::Boolean},
    {"--no-delay", OptionType::Boolean},
     * to:
    {"-D", OptionType::Value},
    {"--delay", OptionType::Value},
     * ... So just ban it until the new option is universal on ChromeOS perf.
     */
    {"-u", OptionType::Value},
    {"--uid", OptionType::Value},
    {"-b", OptionType::Boolean},
    {"--branch-any", OptionType::Boolean},
    {"-j", OptionType::Value},
    {"--branch-filter", OptionType::Value},
    {"-W", OptionType::Boolean},
    {"--weight", OptionType::Boolean},
    {"--transaction", OptionType::Boolean},
    /* Banned: {"--per-thread", OptionType::Boolean},
     * Only briefly present in v3.12-v3.13, but also banned:
     * {"--force-per-cpu", OptionType::Boolean}, */
    /* Banned: {"-I", OptionType::Boolean},  // may reveal PII
    {"--intr-regs", OptionType::Boolean}, */
    {"--running-time", OptionType::Boolean},
    {"-k", OptionType::Value},
    {"--clockid", OptionType::Value},
    {"-S", OptionType::Value},
    {"--snapshot", OptionType::Value},

    {"--pfm-events", OptionType::Value},
  };
  return *kPerfRecordOptions;
}

const std::map<string, OptionType>& GetPerfStatOptions() {
  static const auto* kPerfStatOptions = new std::map<string, OptionType> {
    {"-T", OptionType::Boolean},
    {"--transaction", OptionType::Boolean},
    {"-e", OptionType::Value},
    {"--event", OptionType::Value},
    {"--filter", OptionType::Value},
    {"-i", OptionType::Boolean},
    {"--no-inherit", OptionType::Boolean},
    {"-p", OptionType::Value},
    {"--pid", OptionType::Value},
    {"-t", OptionType::Value},
    {"--tid", OptionType::Value},
    {"-a", OptionType::Boolean},
    {"--all-cpus", OptionType::Boolean},
    {"-g", OptionType::Boolean},
    {"--group", OptionType::Boolean},
    {"-c", OptionType::Boolean},
    {"--scale", OptionType::Boolean},
    /* Banned: {"-v", OptionType::Boolean},
     * {"--verbose", OptionType::Boolean}, */
    /* Banned: {"-r", OptionType::Value},
     * {"--repeat", OptionType::Value}, */
    /* Banned: {"-n", OptionType::Boolean},
     * {"--null", OptionType::Boolean}, */
    /* Banned: {"-d", OptionType::Boolean},
     * {"--detailed", OptionType::Boolean}, */
    /* Banned: {"-S", OptionType::Boolean},
     * {"--sync", OptionType::Boolean}, */
    /* Banned: {"-B", OptionType::Boolean},
     * {"--big-num", OptionType::Boolean}, */
    {"-C", OptionType::Value},
    {"--cpu", OptionType::Value},
    {"-A", OptionType::Boolean},
    {"--no-aggr", OptionType::Boolean},
    /* Banned: {"-x", OptionType::Value},
     * {"--field-separator", OptionType::Value}, */
    {"-G", OptionType::Value},
    {"--cgroup", OptionType::Value},
    /* Banned: {"-o", OptionType::Value},
     * {"--output", OptionType::Value}, */
    /* Banned: {"--append", OptionType::Value}, */
    /* Banned: {"--log-fd", OptionType::Value}, */
    /* Banned: {"--pre", OptionType::Value}, */
    /* Banned: {"--post", OptionType::Value}, */
    /* Banned: {"-I", OptionType::Value},
     * {"--interval-print", OptionType::Value}, */
    {"--per-socket", OptionType::Boolean},
    {"--per-core", OptionType::Boolean},
    {"-D", OptionType::Value},
    {"--delay", OptionType::Value},
  };
  return *kPerfStatOptions;
}

bool ValidatePerfCommandLineOptions(
    const std::vector<string> &args,
    const std::map<string, OptionType> &options) {
  for (std::size_t i = 2; i < args.size(); i++) {
    const string &arg = args[i];
    const auto &it = options.find(arg);
    if (it == options.end()) {
      return false;
    }
    if (it->second == OptionType::Value) {
      ++i;
    }
  }
  return true;
}

}  // namespace

bool ValidatePerfCommandLine(const std::vector<string> &args) {
  if (args.size() < 2) {
    return false;
  }
  if (args[0] != "perf") {
    return false;
  }
  if (args[1] == "record") {
    return ValidatePerfCommandLineOptions(args, GetPerfRecordOptions());
  }
  if (args[1] == "stat") {
    return ValidatePerfCommandLineOptions(args, GetPerfStatOptions());
  }
  return false;
}

}  // namespace quipper
