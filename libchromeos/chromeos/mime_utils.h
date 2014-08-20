// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_MIME_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_MIME_UTILS_H_

#include <string>
#include <utility>
#include <vector>

#include <base/basictypes.h>

namespace chromeos {
namespace mime {

namespace types {
// Main MIME type categories
extern const char kApplication[];        // application
extern const char kAudio[];              // audio
extern const char kImage[];              // image
extern const char kMessage[];            // message
extern const char kMultipart[];          // multipart
extern const char kText[];               // test
extern const char kVideo[];              // video
}

namespace parameters {
// Common MIME parameters
extern const char kCharset[];            // charset=...
}

namespace image {
// Common image MIME types
extern const char kJpeg[];               // image/jpeg
extern const char kPng[];                // image/png
extern const char kBmp[];                // image/bmp
extern const char kTiff[];               // image/tiff
extern const char kGif[];                // image/gif
}

namespace text {
// Common text MIME types
extern const char kPlain[];              // text/plain
extern const char kHtml[];               // text/html
extern const char kXml[];                // text/xml
}

namespace application {
// Common application MIME types
extern const char kOctet_stream[];       // application/octet-stream
extern const char kJson[];               // application/json
extern const char kWwwFormUrlEncoded[];  // application/x-www-form-urlencoded
}

typedef std::vector<std::pair<std::string, std::string>> Parameters;

// Combine a MIME type, subtype and parameters into a MIME string.
// e.g. Combine("text", "plain", {{"charset", "utf-8"}}) will give:
//      "text/plain; charset=utf-8"
std::string Combine(const std::string& type, const std::string& subtype,
                    const Parameters& parameters = {}) WARN_UNUSED_RESULT;

// Splits a MIME string into type and subtype.
// "text/plain;charset=utf-8" => ("text", "plain")
bool Split(const std::string& mime_string,
           std::string* type, std::string* subtype);

// Splits a MIME string into type, subtype, and parameters.
// "text/plain;charset=utf-8" => ("text", "plain", {{"charset","utf-8"}})
bool Split(const std::string& mime_string,
           std::string* type, std::string* subtype, Parameters* parameters);

// Returns the MIME type from MIME string.
// "text/plain;charset=utf-8" => "text"
std::string GetType(const std::string& mime_string);

// Returns the MIME sub-type from MIME string.
// "text/plain;charset=utf-8" => "plain"
std::string GetSubtype(const std::string& mime_string);

// Returns the MIME parameters from MIME string.
// "text/plain;charset=utf-8" => {{"charset","utf-8"}}
Parameters GetParameters(const std::string& mime_string);

// Removes parameters from a MIME string
// "text/plain;charset=utf-8" => "text/plain"
std::string RemoveParameters(const std::string& mime_string) WARN_UNUSED_RESULT;

// Appends a parameter to a MIME string.
// "text/plain" => "text/plain; charset=utf-8"
std::string AppendParameter(const std::string& mime_string,
                            const std::string& paramName,
                            const std::string& paramValue) WARN_UNUSED_RESULT;

// Returns the value of a parameter on a MIME string (empty string if missing).
// ("text/plain;charset=utf-8","charset") => "utf-8"
std::string GetParameterValue(const std::string& mime_string,
                              const std::string& paramName);

}  // namespace mime
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_MIME_UTILS_H_
