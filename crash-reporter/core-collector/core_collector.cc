// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>
#include <unistd.h>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include <client/linux/minidump_writer/linux_core_dumper.h>
#include <client/linux/minidump_writer/minidump_writer.h>

#include "crash-reporter/core-collector/coredump_writer.h"

int main(int argc, char *argv[]) {
  DEFINE_string(minidump, "dump", "Output minidump");
  DEFINE_string(coredump, "core", "Stripped core dump");
  DEFINE_string(proc, "/tmp", "Temporary directory for generated proc files");
  DEFINE_string(prefix, "", "Root directory to which .so paths are relative");
  DEFINE_bool(syslog, false, "Enable syslog logging");

  brillo::FlagHelper::Init(argc, argv,
      "Generate minidump from core dump piped to standard input.");

  if (FLAGS_syslog) {
    const auto name = base::FilePath(argv[0]).BaseName().value();
    brillo::OpenLog(name.c_str(), true);
    brillo::InitLog(brillo::kLogToSyslog);
  }

  if (isatty(STDIN_FILENO)) {
    LOG(ERROR) << "Core dump must be piped to standard input.";
    return EX_USAGE;
  }

  CoredumpWriter writer(
      STDIN_FILENO, FLAGS_coredump.c_str(), FLAGS_proc.c_str());
  const int error = writer.WriteCoredump();
  if (error != EX_OK) {
    LOG(ERROR) << "Failed to write stripped core dump.";
    return error;
  }

  google_breakpad::MappingList mappings;
  google_breakpad::AppMemoryList memory_list;
  google_breakpad::LinuxCoreDumper dumper(-1,  // PID is not used.
      FLAGS_coredump.c_str(), FLAGS_proc.c_str(), FLAGS_prefix.c_str());
  if (!WriteMinidump(FLAGS_minidump.c_str(), mappings, memory_list, &dumper)) {
    LOG(ERROR) << "Failed to convert core dump to minidump.";
    return EX_SOFTWARE;
  }

  return EX_OK;
}
