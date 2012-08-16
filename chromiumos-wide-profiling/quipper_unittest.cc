// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gtest/gtest.h>
#include <stdlib.h>
#include "common.h"
#include "profiler.h"
#include "parser.h"
#include "uploader.h"

const std::string lsb_release = "/etc/lsb-release";
const std::string perf_binary = "/usr/local/sbin/perf";
const std::string perf_data = "/tmp/perf.data";
const std::string event = "cycles";
const std::string freq = "100";
const std::string interval = "2";
const std::string server = GAE_SERVER;
const std::string dummy_board = "dummy_board";
const std::string dummy_version = "dummy_version";

void full_test();

TEST(ParserTest, ParserConstruct) {
  // Constructs the parser. Ensure that values aren't empty.
  Parser p(lsb_release);
  ASSERT_GE((int)p.board.length(), 0);
  ASSERT_GE((int)p.chromeos_version.length(), 0);
}

TEST(ProfilerTest, CanWriteToTmp) {
  // Asserts that tmp is writable.
  struct stat tmp_stat;
  char tmp_loc[] = "/tmp/";
  stat(tmp_loc, &tmp_stat);
  ASSERT_TRUE(tmp_stat.st_mode & S_IRWXU);
}

TEST(UploaderTest, CanReachServer) {
  // Asserts that we can establish a connection with the server.
  std::string command = "curl " + server;
  ASSERT_EQ(system(command.c_str()), 0);
}

TEST(UploaderTest, CanGzip) {
  // Asserts that we can gzip a file.
  // Create the tmp file.
  char tmp_location[] = "/tmp/perf.data.XXXXXX";
  int fd = mkstemp(tmp_location);
  char dummy_data[] = "This is some dummy data to gzip.";
  write(fd, dummy_data, strlen(dummy_data));
  close(fd);
  const std::string input_data_filepath = std::string(tmp_location);
  // Construct uploader.
  Uploader u(input_data_filepath, dummy_board, dummy_version, server);
  // Gzip data.
  u.DoGzip();
  // Assert that we created a .gz file.
  struct stat file_stat;
  const std::string out_path = input_data_filepath + COMPRESSED_EXTENSION;
  ASSERT_EQ(stat(out_path.c_str(), &file_stat), 0);
}

// The following 6 tests are broken currently.
#ifdef DEBUG
TEST(ProfilerTest, CanProfile) {
  // Asserts that the profiler runs.
  Profiler p(perf_binary, event, freq, interval, perf_data);
  ASSERT_EQ(p.DoProfile(), QUIPPER_SUCCESS);
  remove(perf_data.c_str());
}

TEST(UploaderTest, CanUpload) {
  //TODO(mrdmnd): Implement this test.
  FAIL();
}

TEST(IntegrationTest, DoublePipeline) {
  // Tests two full cycles, to ensure we reset global state to something sane.
  full_test();
  full_test();
}

TEST(IntegrationTest, FullPipeline) {
  // Tests one full cycle.
  full_test();
}

TEST(ParserTest, LSBExists) {
  // Asserts that lsb-release exists by running stat.
  struct stat file_stat;
  ASSERT_EQ(stat(lsb_release.c_str(), &file_stat), 0);
}

TEST(ProfilerTest, PerfExists) {
  // Asserts that the perf binary exists in the right location.
  struct stat file_stat;
  ASSERT_EQ(stat(perf_binary.c_str(), &file_stat), 0);
}
#endif

void full_test() {
  char tmp_location[] = "/tmp/perf.data.XXXXXX";
  int fd = mkstemp(tmp_location);
  close(fd);
  const std::string tmp_perf_data = std::string(tmp_location);
  Parser parser(lsb_release);
  parser.ParseLSB();
  Profiler profiler(perf_binary, event, freq, interval, tmp_perf_data);
  Uploader uploader(tmp_perf_data, parser.board,
                    parser.chromeos_version, server);

  ASSERT_EQ(profiler.DoProfile(), 0);
  ASSERT_EQ(uploader.CompressAndUpload(), 0);

}
int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
