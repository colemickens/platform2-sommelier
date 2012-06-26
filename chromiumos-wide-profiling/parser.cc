// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <fstream>
#include <syslog.h>
#include "common.h"
#include "parser.h"

Parser::Parser(const std::string& lsb_release) : lsb_release_(lsb_release) {}

void Parser::GetLines(std::vector<std::string>& strings) {
  // Stores the lines read from lsb-release into the strings input param.
  std::ifstream lsb_stream(lsb_release_.c_str());
  if (lsb_stream.is_open()) {
    std::string line;
    while (lsb_stream.good()) {
      getline(lsb_stream, line);
      strings.push_back(line);
    }
  }
  lsb_stream.close();
}

void Parser::GetValueFromKey(const std::string& key,
                             const std::vector<std::string>& lines,
                             std::string& output) {
  // Searches through a vector of lines for a string that begins with key,
  // returns the text after the equals sign if found, or empty string if not.
  // If the value IS the empty string, we'll return it anyways.
  // Places result in output parameter.
  for (size_t i = 0; i < lines.size(); i++) {
    std::string line = lines[i];
    if (line.find(key) != std::string::npos) {
      size_t equals_pos = line.find("=");
      output = line.substr(equals_pos+1, std::string::npos);
      return;
    }
  }
  output = "";
}

void Parser::ParseLSB() {
  // A method that only needs to be called once.
  // Populates two member variables if they aren't already populated.
  // Read in the lsb_release file, parse out the appropriate data.
  if (board.size() != 0 || this->chromeos_version.size() != 0) {
    // Data already parsed, do no work.
    return;
  }
  // No stored data, we should try to parse.
  std::vector<std::string> lines;
  GetLines(lines);
  GetValueFromKey("CHROMEOS_RELEASE_BOARD", lines, board);
  GetValueFromKey("CHROMEOS_RELEASE_VERSION", lines, chromeos_version);
  if (board.size() == 0) {
    board = UNKNOWN_MACHINE_DETAIL;
  }
  if (chromeos_version.size() == 0) {
    chromeos_version = UNKNOWN_MACHINE_DETAIL;
  }
}
