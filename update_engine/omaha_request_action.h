// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_ACTION_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_ACTION_H__

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <string>

#include <curl/curl.h>

#include "base/scoped_ptr.h"
#include "update_engine/action.h"
#include "update_engine/http_fetcher.h"

// The Omaha Request action makes a request to Omaha and can output
// the response on the output ActionPipe.

namespace chromeos_update_engine {

// Encodes XML entities in a given string with libxml2. input must be
// UTF-8 formatted. Output will be UTF-8 formatted.
std::string XmlEncode(const std::string& input);

// This struct encapsulates the data Omaha's response for the request.
// These strings in this struct are not XML escaped.
struct OmahaResponse {
  OmahaResponse()
      : update_exists(false), size(0), needs_admin(false), prompt(false) {}
  // True iff there is an update to be downloaded.
  bool update_exists;

  // These are only valid if update_exists is true:
  std::string display_version;
  std::string codebase;
  std::string more_info_url;
  std::string hash;
  off_t size;
  bool needs_admin;
  bool prompt;
  bool is_delta;
};
COMPILE_ASSERT(sizeof(off_t) == 8, off_t_not_64bit);

// This struct encapsulates the Omaha event information. For a
// complete list of defined event types and results, see
// http://code.google.com/p/omaha/wiki/ServerProtocol#event
struct OmahaEvent {
  enum Type {
    kTypeUnknown = 0,
    kTypeDownloadComplete = 1,
    kTypeInstallComplete = 2,
    kTypeUpdateComplete = 3,
    kTypeUpdateDownloadStarted = 13,
    kTypeUpdateDownloadFinished = 14,
  };

  enum Result {
    kResultError = 0,
    kResultSuccess = 1,
  };

  OmahaEvent()
      : type(kTypeUnknown),
        result(kResultError),
        error_code(0) {}
  OmahaEvent(Type in_type, Result in_result, int in_error_code)
      : type(in_type),
        result(in_result),
        error_code(in_error_code) {}

  Type type;
  Result result;
  int error_code;
};

class NoneType;
class OmahaRequestAction;
struct OmahaRequestParams;

template<>
class ActionTraits<OmahaRequestAction> {
 public:
  // Takes parameters on the input pipe.
  typedef NoneType InputObjectType;
  // On UpdateCheck success, puts the Omaha response on output. Event
  // requests do not have an output pipe.
  typedef OmahaResponse OutputObjectType;
};

class OmahaRequestAction : public Action<OmahaRequestAction>,
                           public HttpFetcherDelegate {
 public:
  // The ctor takes in all the parameters that will be used for making
  // the request to Omaha. For some of them we have constants that
  // should be used.
  //
  // Takes ownership of the passed in HttpFetcher. Useful for testing.
  //
  // Takes ownership of the passed in OmahaEvent. If |event| is NULL,
  // this is an UpdateCheck request, otherwise it's an Event request.
  // Event requests always succeed.
  //
  // A good calling pattern is:
  // OmahaRequestAction(..., new OmahaEvent(...), new WhateverHttpFetcher);
  // or
  // OmahaRequestAction(..., NULL, new WhateverHttpFetcher);
  OmahaRequestAction(const OmahaRequestParams& params,
                     OmahaEvent* event,
                     HttpFetcher* http_fetcher);
  virtual ~OmahaRequestAction();
  typedef ActionTraits<OmahaRequestAction>::InputObjectType InputObjectType;
  typedef ActionTraits<OmahaRequestAction>::OutputObjectType OutputObjectType;
  void PerformAction();
  void TerminateProcessing();

  // Debugging/logging
  static std::string StaticType() { return "OmahaRequestAction"; }
  std::string Type() const { return StaticType(); }

  // Delegate methods (see http_fetcher.h)
  virtual void ReceivedBytes(HttpFetcher *fetcher,
                             const char* bytes, int length);
  virtual void TransferComplete(HttpFetcher *fetcher, bool successful);

  // Returns true if this is an Event request, false if it's an UpdateCheck.
  bool IsEvent() const { return event_.get() != NULL; }

 private:
  // These are data that are passed in the request to the Omaha server.
  const OmahaRequestParams& params_;

  // Pointer to the OmahaEvent info. This is an UpdateCheck request if NULL.
  scoped_ptr<OmahaEvent> event_;

  // pointer to the HttpFetcher that does the http work
  scoped_ptr<HttpFetcher> http_fetcher_;

  // Stores the response from the omaha server
  std::vector<char> response_buffer_;

  DISALLOW_COPY_AND_ASSIGN(OmahaRequestAction);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_OMAHA_REQUEST_ACTION_H__
