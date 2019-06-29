// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBMEMS_TEST_FAKES_H_
#define LIBMEMS_TEST_FAKES_H_

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "libmems/export.h"
#include "libmems/iio_channel.h"
#include "libmems/iio_context.h"
#include "libmems/iio_device.h"

namespace libmems {
namespace fakes {
class LIBMEMS_EXPORT FakeIioChannel : public IioChannel {
 public:
  FakeIioChannel(const std::string& id, bool enabled);

  const char* GetId() const override { return id_.c_str(); }

  bool IsEnabled() const override { return enabled_; }
  bool SetEnabled(bool en) override;

 private:
  std::string id_;
  bool enabled_;
};

class FakeIioContext;

class LIBMEMS_EXPORT FakeIioDevice : public IioDevice {
 public:
  FakeIioDevice(FakeIioContext* ctx,
                const std::string& name,
                const std::string& id);

  IioContext* GetContext() const override {
    return reinterpret_cast<IioContext*>(context_);
  }

  base::FilePath GetPath() const override {
    return base::FilePath("/sys/bus/iio/devices").Append(GetId());
  }

  iio_device* GetUnderlyingIioDevice() const override { return nullptr; }

  const char* GetName() const override { return name_.c_str(); }
  const char* GetId() const override { return id_.c_str(); }

  base::Optional<std::string> ReadStringAttribute(
      const std::string& name) const override;
  base::Optional<int64_t> ReadNumberAttribute(
      const std::string& name) const override;

  bool WriteStringAttribute(const std::string& name,
                            const std::string& value) override;
  bool WriteNumberAttribute(const std::string& name, int64_t value) override;

  bool SetTrigger(IioDevice* trigger) override;
  IioDevice* GetTrigger() override { return trigger_; }

  void AddChannel(IioChannel* chan) { channels_.emplace(chan->GetId(), chan); }

  IioChannel* GetChannel(const std::string& id) override;

  bool EnableBuffer(size_t n) override;
  bool DisableBuffer() override;
  bool IsBufferEnabled(size_t* n = nullptr) const override;

 private:
  FakeIioContext* context_;
  std::string name_;
  std::string id_;
  std::map<std::string, int> numeric_attributes_;
  std::map<std::string, std::string> text_attributes_;
  IioDevice* trigger_;
  std::map<std::string, IioChannel*> channels_;
  size_t buffer_length_ = 0;
  bool buffer_enabled_ = false;
};

class LIBMEMS_EXPORT FakeIioContext : public IioContext {
 public:
  FakeIioContext() = default;

  void AddDevice(FakeIioDevice* device);

  void Reload() override {}

  IioDevice* GetDevice(const std::string& name) override;

 private:
  std::map<std::string, FakeIioDevice*> devices_;
};

}  // namespace fakes

}  // namespace libmems

#endif  // LIBMEMS_TEST_FAKES_H_
