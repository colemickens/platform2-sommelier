// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metrics/process_meter.h"

#include <memory>

#include <gtest/gtest.h>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

namespace chromeos_metrics {

class ProcessMeterTest : public testing::Test {};

void CreateFile(const base::FilePath& path, std::string content) {
  if (base::WriteFile(path, content.c_str(), content.length()) !=
      content.length()) {
    LOG(FATAL) << "cannot write to " << path.MaybeAsASCII();
  }
}

void CreateProcEntry(const base::FilePath& procfs_path,
                     int pid,
                     int ppid,
                     const char* name,
                     const char* cmdline,
                     int total_mib,
                     int anon_mib,
                     int file_mib,
                     int shmem_mib,
                     int swap_mib) {
  base::FilePath proc_pid_path(
      procfs_path.Append(base::StringPrintf("%d", pid)));
  base::FilePath status_path(proc_pid_path.Append("status"));
  base::FilePath statm_path(proc_pid_path.Append("statm"));
  base::FilePath stat_path(proc_pid_path.Append("stat"));
  base::FilePath cmdline_path(proc_pid_path.Append("cmdline"));
  std::string stat_content =
      base::StringPrintf("%d %s R %d f4 f5 f6 f7 f8 f9 110", pid, name, ppid);
  bool is_kdaemon = total_mib == 0;
  std::string status_content =
      is_kdaemon ? "blah\nblah\nblah"
                 : base::StringPrintf(
                       "blah\nblah\nblah\n"
                       "VmRSS:      %d kB\n"
                       "RssAnon:    %d kB\n"
                       "RssFile:     %d kB\n"
                       "RssShmem:    %d kB\n"
                       "VmSwap:    %d kB\n"
                       "blah\nblah\nblah\n",
                       total_mib * 1024, anon_mib * 1024, file_mib * 1024,
                       shmem_mib * 1024, swap_mib * 1024);
  // statm fields: size resident shared text lib data dt.  Units: 4k pages.
  std::string statm_content =
      is_kdaemon ? "0 0 0 0 0 0 0\n"
                 : base::StringPrintf("999999 %d %d 5 0 2 0\n", total_mib * 256,
                                      shmem_mib * 256);
  CHECK(CreateDirectory(proc_pid_path));
  CreateFile(stat_path, stat_content);
  CreateFile(statm_path, statm_content);
  CreateFile(status_path, status_content);
  CreateFile(cmdline_path, std::string(cmdline));
}

// Test that we're classifying processes and adding up their sizes correctly.
TEST_F(ProcessMeterTest, ReportProcessStats) {
  base::FilePath temp_dir;
  EXPECT_TRUE(base::CreateNewTempDirectory("", &temp_dir));
  base::FilePath run_path = temp_dir.Append("run");
  base::FilePath procfs_path = temp_dir.Append("proc");

  // Create arc init PID file in mock /run.
  const int arc_init_pid = 22;
  base::FilePath arc_init_path = run_path.Append(kMetricsARCInitPIDFile);
  CHECK(CreateDirectory(arc_init_path.DirName()));
  const std::string arc_init_pid_string =
      base::StringPrintf("%d", arc_init_pid);
  const char* s = arc_init_pid_string.c_str();
  CreateFile(arc_init_path, s);

  // Create mock /proc.
  CHECK(CreateDirectory(procfs_path));

  // Fill /proc with entries for a few processes.
  // clang-format off

  // init.
  CreateProcEntry(procfs_path, 1, 0, "init", "/sbin/init",
                  10, 5, 5, 0, 7);
  // ARC init.
  CreateProcEntry(procfs_path, arc_init_pid, 1, "arc-init", "/blah/arc/init",
                  10, 5, 5, 0, 1);
  // kthreadd (kernel daemon)
  CreateProcEntry(procfs_path, 2, 0, "kthreadd", "",
                  0, 0, 0, 0, 0);
  // Browser processes.
  CreateProcEntry(procfs_path, 100, 1, "chrome",
                  "/opt/google/chrome/chrome blah",
                  300, 200, 90, 10, 2);
  CreateProcEntry(procfs_path, 101, 100, "chrome",
                  "/opt/google/chrome/chrome --type=broker",
                  5, 4, 3, 2, 1);
  // GPU.
  CreateProcEntry(procfs_path, 110, 100, "chrome",
                  "/opt/google/chrome/chrome --type=gpu-process",
                  400, 70, 30, 300, 3);
  // Renderers.
  CreateProcEntry(procfs_path, 120, 100, "chrome",
                  "/opt/google/chrome/chrome --type=renderer",
                  500, 450, 30, 20, 13);
  CreateProcEntry(procfs_path, 121, 100, "chrome",
                  "/opt/google/chrome/chrome --type=renderer",
                  500, 450, 30, 20,  13);
  // Daemons.
  CreateProcEntry(procfs_path, 200, 1, "shill", "/usr/bin/shill",
                  100, 30, 70, 0, 0);
  // clang-format on

  // Get process info from mocked /proc.
  ProcessInfo info(procfs_path, run_path);
  info.Collect();
  info.Classify();
  const uint64_t mib = 1 << 20;
  // clang-format off
  const ProcessMemoryStats expected_stats[PG_KINDS_COUNT] = {
      // browser
      {{ 305 * mib, 204 * mib, 93 * mib,  12 * mib,  3 * mib}},
      // gpu
      {{ 400 * mib,  70 * mib, 30 * mib, 300 * mib,  3 * mib}},
      // renderers
      {{1000 * mib, 900 * mib, 60 * mib,  40 * mib, 26 * mib}},
      // arc
      {{  10 * mib,   5 * mib,  5 * mib,  0,         1 * mib}},
      // vms
      {{   0,         0,        0,        0,         0}},
      // daemons
      {{110 * mib,   35 * mib, 75 * mib,  0,         7 * mib}},
  };
  // clang-format on
  for (int i = 0; i < PG_KINDS_COUNT; i++) {
    ProcessMemoryStats stats;
    ProcessGroupKind kind = static_cast<ProcessGroupKind>(i);
    AccumulateProcessGroupStats(procfs_path, info.GetGroup(kind), true, &stats);
    for (int j = 0; j < MEM_KINDS_COUNT; j++) {
      EXPECT_EQ(stats.rss_sizes[j], expected_stats[i].rss_sizes[j]);
    }
  }
}

void CheckPG(int pg, const char* field) {
  for (int i = 0; i < MEM_KINDS_COUNT; i++) {
    CHECK(strcasestr(kProcessMemoryUMANames[pg][i], field) != NULL);
  }
}

void CheckMem(int mem, const char* field) {
  for (int i = 0; i < PG_KINDS_COUNT; i++) {
    CHECK(strcasestr(kProcessMemoryUMANames[i][mem], field) != NULL);
  }
}

// Test that the enum constants for process kind and memory kind match the UMA
// histogram names.
TEST_F(ProcessMeterTest, CheckUMANames) {
  CheckPG(PG_BROWSER, "browser");
  CheckPG(PG_GPU, "gpu");
  CheckPG(PG_RENDERERS, "renderers");
  CheckPG(PG_ARC, "arc");
  CheckPG(PG_VMS, "vms");
  CheckPG(PG_DAEMONS, "daemons");

  CheckMem(MEM_TOTAL, "total");
  CheckMem(MEM_ANON, "anon");
  CheckMem(MEM_FILE, "file");
  CheckMem(MEM_SHMEM, "shmem");
  CheckMem(MEM_SWAP, "swap");

  // Extra consistency checks.
  ProcessMemoryStats stats;
  CHECK_EQ(arraysize(stats.rss_sizes), arraysize(kProcessMemoryUMANames[0]));
  CHECK_EQ(arraysize(kProcessMemoryUMANames), PG_KINDS_COUNT);
}

}  // namespace chromeos_metrics
