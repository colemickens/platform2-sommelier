// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBMEMS_IIO_DEVICE_IMPL_H_
#define LIBMEMS_IIO_DEVICE_IMPL_H_

#include <iio.h>

#include <map>
#include <memory>
#include <string>

#include <base/optional.h>

#include "libmems/export.h"
#include "libmems/iio_device.h"

namespace libmems {

class IioChannelImpl;
class IioContext;
class IioContextImpl;

class LIBMEMS_EXPORT IioDeviceImpl : public IioDevice {
 public:
  // iio_device objects are kept alive by the IioContextImpl.
  IioDeviceImpl(IioContextImpl* ctx, iio_device* dev);
  ~IioDeviceImpl() override = default;

  IioContext* GetContext() const override;

  const char* GetName() const override;
  const char* GetId() const override;

  base::FilePath GetPath() const override;

  base::Optional<std::string> ReadStringAttribute(
      const std::string& name) const override;
  base::Optional<int64_t> ReadNumberAttribute(
      const std::string& name) const override;

  bool WriteStringAttribute(const std::string& name,
                            const std::string& value) override;
  bool WriteNumberAttribute(const std::string& name, int64_t value) override;

  iio_device* GetUnderlyingIioDevice() const override;

  bool SetTrigger(IioDevice* trigger_device) override;
  IioDevice* GetTrigger() override;

  IioChannel* GetChannel(const std::string& name) override;

  bool EnableBuffer(size_t num) override;
  bool DisableBuffer() override;
  bool IsBufferEnabled(size_t* num = nullptr) const override;

 private:
  IioContextImpl* context_;  // non-owned
  iio_device* const device_;       // non-owned
  std::map<std::string, std::unique_ptr<IioChannelImpl>> channels_;
  DISALLOW_COPY_AND_ASSIGN(IioDeviceImpl);
};

}  // namespace libmems

#endif  // LIBMEMS_IIO_DEVICE_IMPL_H_