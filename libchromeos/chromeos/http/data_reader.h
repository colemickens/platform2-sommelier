// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_DATA_READER_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_DATA_READER_H_

#include <string>
#include <vector>

#include <chromeos/chromeos_export.h>
#include <chromeos/errors/error.h>

namespace chromeos {
namespace http {

// Abstract data reading interface for HTTP transport.
class DataReaderInterface {
 public:
  virtual ~DataReaderInterface() = default;

  // Returns the size of data under the control of the data reader.
  virtual uint64_t GetDataSize() const = 0;

  // Reads up to |max_size_to_read| bytes from the reader into the provided
  // |buffer| and returns true on success as well as the actual number of bytes
  // read through |size_read| which may be less than |size_to_read| requested.
  // If end of data is reached, the function returns true and sets |size_read|
  // to 0.
  virtual bool ReadData(void* buffer,
                        size_t max_size_to_read,
                        size_t* size_read,
                        chromeos::ErrorPtr* error) = 0;
};

// A DataReaderInterface implementation for memory buffers.
class CHROMEOS_EXPORT MemoryDataReader final : public DataReaderInterface {
 public:
  MemoryDataReader() = default;
  explicit MemoryDataReader(const std::string& data);
  MemoryDataReader(const void* data, size_t data_size);

  // Reset the data to new value and rewinds the read pointer to the
  // beginning of data buffer.
  void SetData(const std::string& data);
  void SetData(const void* data, size_t data_size);

  // Overrides from DataReaderInterface.
  uint64_t GetDataSize() const override { return data_.size(); }
  bool ReadData(void* buffer,
                size_t size_to_read,
                size_t* size_read,
                chromeos::ErrorPtr* error) override;

 private:
  friend class MemoryDataReaderTest;

  // The memory buffer to read data from.
  std::vector<uint8_t> data_;
  // The current read offset from the beginning of the buffer.
  size_t read_pointer_{0};

  DISALLOW_COPY_AND_ASSIGN(MemoryDataReader);
};

}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_DATA_READER_H_
