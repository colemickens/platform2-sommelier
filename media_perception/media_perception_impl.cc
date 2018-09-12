// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_perception/media_perception_impl.h"

#include <utility>

#include "base/logging.h"

namespace mri {

MediaPerceptionImpl::MediaPerceptionImpl(
    chromeos::media_perception::mojom::MediaPerceptionRequest request)
    : binding_(this, std::move(request)) {}

void MediaPerceptionImpl::set_connection_error_handler(
    base::Closure connection_error_handler) {
  binding_.set_connection_error_handler(std::move(connection_error_handler));
}

}  // namespace mri
