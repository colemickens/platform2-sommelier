// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARSER_H
#define PARSER_H
#include <string>
#include <vector>

class Parser {
  // A class to read and parse /etc/lsb-release if it exists, or identify
  // chromeos_version and board some other way.
  private:
    const std::string lsb_release_;
    Parser() {};
    Parser(const Parser& other) {};
    void GetValueFromKey(const std::string& key,
                         const std::vector<std::string>& lines,
                         std::string& output);
    void GetLines(std::vector<std::string>& strings);
  public:
    explicit Parser(const std::string& lsb_release);
    void ParseLSB();
    std::string chromeos_version;
    std::string board;
};
#endif
