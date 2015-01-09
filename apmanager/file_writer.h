// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_FILE_WRITER_H_
#define APMANAGER_FILE_WRITER_H_

#include <string>

#include <base/lazy_instance.h>

namespace apmanager {

// Singleton class for handling file writes.
class FileWriter {
 public:
  virtual ~FileWriter();

  // This is a singleton. Use FileWriter::GetInstance()->Foo().
  static FileWriter* GetInstance();

  virtual bool Write(const std::string& file_name,
                     const std::string& content);

 protected:
  FileWriter();

 private:
  friend struct base::DefaultLazyInstanceTraits<FileWriter>;

  DISALLOW_COPY_AND_ASSIGN(FileWriter);
};

}  // namespace apmanager

#endif  // APMANAGER_FILE_WRITER_H_
