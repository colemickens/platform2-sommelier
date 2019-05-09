// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "mems_setup/iio_channel_impl.h"
#include "mems_setup/iio_device.h"

namespace mems_setup {

IioChannelImpl::IioChannelImpl(iio_channel* channel) : channel_(channel) {
  CHECK(channel_);
}

const char* IioChannelImpl::GetId() const {
  return iio_channel_get_id(channel_);
}

bool IioChannelImpl::IsEnabled() const {
  return iio_channel_is_enabled(channel_);
}

bool IioChannelImpl::SetEnabled(bool en) {
  if (en)
    iio_channel_enable(channel_);
  else
    iio_channel_disable(channel_);

  // this tool will not stick around listening to this channel,
  // all it needs to do is leave the channel enabled for Chrome to use;
  // so, we directly write to the scan elements instead of setting up
  // a buffer and keeping it enabled while we run (which wouldn't be long
  // enough anyway). we do not need to handle the non scan-element case for
  // the channels we care about.
  if (!iio_channel_is_scan_element(channel_))
    return true;

  std::string en_attrib_name = base::StringPrintf(
      "scan_elements/%s_%s_en",
      iio_channel_is_output(channel_) ? "out" : "in", GetId());
  int ok = iio_channel_attr_write_bool(channel_, en_attrib_name.c_str(), en);
  if (!ok) {
    LOG(WARNING) << "could not write to " << en_attrib_name
                  << ", error: " << ok;
    return false;
  }

  return true;
}

}  // namespace mems_setup
