// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERF_TEST_FILES_
#define PERF_TEST_FILES_

namespace perf_test_files {

// The following perf data contains the following event types, as passed to
// perf record via the -e option:
// - cycles
// - instructions
// - cache-references
// - cache-misses
// - branches
// - branch-misses

const char* kPerfDataFiles[] = {
  // Obtained with "perf record -- echo > /dev/null"
  "perf.data.singleprocess",

  // Obtained with "perf record -a -- sleep $N", for N in {0, 1, 5}.
  "perf.data.systemwide.0",
  "perf.data.systemwide.1",
  "perf.data.systemwide.5",

  // Obtained with "perf record -a -- sleep $N", for N in {0, 1, 5}.
  // While in the background, this loop is running:
  //   while true; do ls > /dev/null; done
  "perf.data.busy.0",
  "perf.data.busy.1",
  "perf.data.busy.5",

  // Obtained with "perf record -a -- sleep 2"
  // While in the background, this loop is running:
  //   while true; do restart powerd; sleep .2; done
  "perf.data.forkexit",
  "perf.data.callgraph",
  "perf.data.branch",
  "perf.data.callgraph_and_branch",

  // Same as above, obtained from a system running kernel-next.
  "perf.data.singleprocess.next",
  "perf.data.systemwide.0.next",
  "perf.data.systemwide.1.next",
  "perf.data.systemwide.5.next",
  "perf.data.busy.0.next",
  "perf.data.busy.1.next",
  "perf.data.busy.5.next",
  "perf.data.forkexit.next",
  "perf.data.callgraph.next",
  "perf.data.branch.next",
  "perf.data.callgraph_and_branch.next",
};

const char* kPerfPipedDataFiles[] = {
  "perf.data.piped.host",
  "perf.data.piped.target",
  "perf.data.piped.powerpc",
  // From system running kernel-next.
  "perf.data.piped.target.next",
};

}  // namespace perf_test_files

#endif  // PERF_TEST_FILES_
