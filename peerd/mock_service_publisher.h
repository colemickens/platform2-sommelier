// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MOCK_SERVICE_PUBLISHER_H_
#define PEERD_MOCK_SERVICE_PUBLISHER_H_

#include <string>

#include <base/memory/weak_ptr.h>
#include <gmock/gmock.h>

#include "peerd/service_publisher_interface.h"

namespace peerd {

class MockServicePublisher : public ServicePublisherInterface {
 public:
  MockServicePublisher() = default;

  MOCK_METHOD2(OnServiceUpdated, bool(chromeos::ErrorPtr* error,
                                      const Service& service));
  MOCK_METHOD2(OnServiceRemoved, bool(chromeos::ErrorPtr* error,
                                      const std::string& service_id));
  MOCK_METHOD2(OnFriendlyNameChanged, bool(chromeos::ErrorPtr* error,
                                           const std::string& name));
  MOCK_METHOD2(OnNoteChanged, bool(chromeos::ErrorPtr* error,
                                   const std::string& note));

  // Must be last member of MockServicePublisher.
  base::WeakPtrFactory<MockServicePublisher> weak_ptr_factory_{this};
};

}  // namespace peerd

#endif  // PEERD_MOCK_SERVICE_PUBLISHER_H_
