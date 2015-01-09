// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_FILE_WRITER_H_
#define APMANAGER_MOCK_FILE_WRITER_H_

#include <string>

#include <base/lazy_instance.h>
#include <gmock/gmock.h>

#include "apmanager/file_writer.h"

namespace apmanager {

class MockFileWriter : public FileWriter {
 public:
  ~MockFileWriter() override;

  // This is a singleton. Use MockFileWriter::GetInstance()->Foo().
  static MockFileWriter* GetInstance();

  MOCK_METHOD2(Write, bool(const std::string& file_name,
                           const std::string& content));

 protected:
  MockFileWriter();

 private:
  friend struct base::DefaultLazyInstanceTraits<MockFileWriter>;

  DISALLOW_COPY_AND_ASSIGN(MockFileWriter);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_FILE_WRITER_H_
