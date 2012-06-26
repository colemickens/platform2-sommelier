// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPLOADER_H
#define UPLOADER_H
#include <string>
class Uploader {
  // A class to send a POST request to the /upload link, then remove the
  // perf.data.gz file.
  private:
    const std::string input_data_file_;
    const std::string output_data_file_;
    const std::string board_;
    const std::string chromeos_version_;
    const std::string server_url_;
    Uploader() {};
    Uploader(const Uploader& other) {};
  public:
    Uploader(const std::string& input_data_file,
             const std::string& board,
             const std::string& chromeos_version,
             const std::string& server_url);
    int CompressAndUpload();
    int DoUpload();
    int DoGzip();

};
#endif
