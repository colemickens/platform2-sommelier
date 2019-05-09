// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEMS_SETUP_IIO_CONTEXT_IMPL_H_
#define MEMS_SETUP_IIO_CONTEXT_IMPL_H_

#include <iio.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mems_setup/iio_context.h"

namespace mems_setup {

class IioContextImpl : public IioContext {
 public:
  IioContextImpl();
  ~IioContextImpl() override = default;

  void Reload() override;
  IioDevice* GetDevice(const std::string& name) override;

 private:
  using ContextUniquePtr =
      std::unique_ptr<iio_context, decltype(&iio_context_destroy)>;

  iio_context* GetCurrentContext() const;

  std::map<std::string, std::unique_ptr<IioDevice>> devices_;
  std::vector<ContextUniquePtr> context_;

  DISALLOW_COPY_AND_ASSIGN(IioContextImpl);
};

}  // namespace mems_setup

#endif  // MEMS_SETUP_IIO_CONTEXT_IMPL_H_
