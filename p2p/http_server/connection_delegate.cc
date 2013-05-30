// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "http_server/connection_delegate.h"
#include "http_server/server.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <attr/xattr.h>

#include <cctype>
#include <cinttypes>
#include <cassert>
#include <cerrno>
#include <string>
#include <algorithm>
#include <vector>
#include <iomanip>

#include <base/time.h>
#include <base/logging.h>
#include <base/file_path.h>

using std::map;
using std::string;
using std::vector;

using base::Time;
using base::TimeDelta;

namespace p2p {

namespace http_server {

ConnectionDelegate::ConnectionDelegate(int dirfd,
                                       int fd,
                                       const string& pretty_addr,
                                       Server* server,
                                       uint64_t max_download_rate)
    : base::DelegateSimpleThread::Delegate(),
      dirfd_(dirfd),
      fd_(fd),
      pretty_addr_(pretty_addr),
      server_(server),
      max_download_rate_(max_download_rate) {
  CHECK(fd_ != -1);
  CHECK(server_ != NULL);
}

ConnectionDelegate::~ConnectionDelegate() { CHECK(fd_ == -1); }

bool ConnectionDelegate::ReadLine(string* str) {
  CHECK(str != NULL);

  while (true) {
    char buf[kLineBufSize];
    ssize_t num_recv;
    int n;

    num_recv = recv(fd_, buf, sizeof buf, MSG_PEEK);
    if (num_recv == -1) {
      LOG(ERROR) << "Error reading: " << strerror(errno);
      return false;
    }
    CHECK_GE(num_recv, 0);

    for (n = 0; n < num_recv; ++n) {
      str->push_back(buf[n]);

      if (buf[n] == '\n') {
        num_recv = recv(fd_, buf, n + 1, 0);  // skip processed data
        CHECK(num_recv == n + 1);
        return true;
      }

      if (str->size() > kMaxLineLength) {
        LOG(ERROR) << "Max line length (" << kMaxLineLength << ") exceeded";
        return false;
      }
    }
    num_recv = recv(fd_, buf, n, 0);  // skip
    CHECK(num_recv == n);
  }
  return true;
}

// Removes "\r\n" from the passed in string. Returns false if
// the string didn't end in "\r\n".
static bool TrimCRLF(string* str) {
  CHECK(str != NULL);
  const char* c = str->c_str();
  size_t len = str->size();
  if (len < 2)
    return false;
  if (strcmp(c + len - 2, "\r\n") != 0)
    return false;
  str->resize(len - 2);
  return true;
}

void ConnectionDelegate::ParseHttpRequest() {
  string request_line;
  map<string, string> headers;
  size_t sp1_pos, sp2_pos;
  string request_method;
  string request_uri;
  string request_http_version;

  if (!ReadLine(&request_line) || !TrimCRLF(&request_line))
    return;

  VLOG(1) << "Request line: `" << request_line << "'";

  sp1_pos = request_line.find(" ");
  if (sp1_pos == string::npos) {
    LOG(ERROR) << "Malformed request line, didn't find starting space"
               << " (request_line=`" << request_line << "')";
    return;
  }
  sp2_pos = request_line.rfind(" ");
  if (sp2_pos == string::npos) {
    LOG(ERROR) << "Malformed request line, didn't find ending space"
               << " (request_line=`" << request_line << "')";
    return;
  }
  if (sp2_pos == sp1_pos) {
    LOG(ERROR) << "Malformed request line, initial space is the same as "
               << "ending space (request_line=`" << request_line << "')";
    return;
  }
  CHECK(sp2_pos > sp1_pos);

  request_method = string(request_line, 0, sp1_pos);
  request_uri = string(request_line, sp1_pos + 1, sp2_pos - sp1_pos - 1);
  request_http_version =
      string(request_line, sp2_pos + 1, string::npos);

  VLOG(1) << "Parsed request line. "
          << "method=`" << request_method << "' "
          << "uri=`" << request_uri << "' "
          << "http_version=`" << request_http_version << "'";

  while (true) {
    string line;
    size_t colon_pos;

    if (!ReadLine(&line) || !TrimCRLF(&line))
      return;

    if (line == "")
      break;

    // TODO(zeuthen): support header continuation. This TODO item is tracked in
    //                https://code.google.com/p/chromium/issues/detail?id=246326
    colon_pos = line.find(": ");
    if (colon_pos == string::npos) {
      LOG(ERROR) << "Malformed HTTP header (line=`" << line << "')";
      return;
    }

    string key = string(line, 0, colon_pos);
    string value = string(line, colon_pos + 2, string::npos);

    // HTTP headers are case-insensitive so lower-case
    std::transform(key.begin(),
                   key.end(),
                   key.begin(),
                   static_cast<int(*)(int c)>(std::tolower));

    VLOG(1) << "Header[" << headers.size() << "] `" << key << "' -> `" << value
            << "'";
    headers[key] = value;

    if (headers.size() == kMaxHeaders) {
      LOG(ERROR) << "Exceeded maximum (" << kMaxHeaders
                 << ") number of HTTP headers";
      return;
    }
  }

  // OK, looks like a valid HTTP request. Service the client.
  ServiceHttpRequest(
      request_method, request_uri, request_http_version, headers);
}

void ConnectionDelegate::Run() {

  ParseHttpRequest();

  if (shutdown(fd_, SHUT_RDWR) != 0) {
    LOG(ERROR) << "Error shutting down socket: " << strerror(errno);
  }
  if (close(fd_) != 0) {
    LOG(ERROR) << "Error closing socket: " << strerror(errno);
  }
  fd_ = -1;

  server_->ConnectionTerminated(this);

  delete this;
}

bool ConnectionDelegate::SendResponse(
    int http_response_code,
    const string& http_response_status,
    const map<string, string>& headers,
    const string& body) {
  string response;
  const char* buf;
  size_t num_to_send;
  size_t num_total_sent;
  size_t body_size = body.size();
  bool has_content_length = false;
  bool has_server = false;

  response = "HTTP/1.1 ";
  response += std::to_string(http_response_code);
  response += " ";
  response += http_response_status;
  response += "\r\n";
  for (auto const& h : headers) {
    response += h.first + ": " + h.second + "\r\n";

    const char* header_name = h.first.c_str();
    if (strcasecmp(header_name, "Content-Length") == 0)
      has_content_length = true;
    else if (strcasecmp(header_name, "Server") == 0)
      has_server = true;
  }

  if (body_size > 0 && !has_content_length) {
    response += string("Content-Length: ");
    response += std::to_string(body_size) + "\r\n";
  }

  if (!has_server)
    response += string("Server: ") + PACKAGE_STRING + "\r\n";

  response += "Connection: close\r\n";
  response += "\r\n";
  response += body;

  buf = response.c_str();
  num_to_send = response.size();
  num_total_sent = 0;
  while (num_to_send > 0) {
    ssize_t num_sent = send(fd_, buf + num_total_sent, num_to_send, 0);
    if (num_sent == -1) {
      LOG(ERROR) << "Error sending: " << strerror(errno);
      return false;
    }
    CHECK_GT(num_sent, 0);
    num_to_send -= num_sent;
    num_total_sent += num_sent;
  }
  return true;
}

/* ------------------------------------------------------------------------ */

bool ConnectionDelegate::SendSimpleResponse(
    int http_response_code,
    const string& http_response_status) {
  map<string, string> headers;
  return SendResponse(http_response_code, http_response_status, headers, "");
}

/* ------------------------------------------------------------------------ */

string ConnectionDelegate::GenerateIndexDotHtml() {
  string body;
  vector<string> files;
  struct dirent* dent;
  struct dirent dent_struct;
  int dirfd_copy;
  DIR* dir;

  body += "<html>\n";
  body += "  <head>\n";
  body += "    <title>P2P files</title>\n";
  body += "  </head>\n";
  body += "  <body>\n";
  body += "    <h1>P2P files</h1>\n";
  body += "    <hr>\n";

  dirfd_copy = dup(dirfd_);
  if (dirfd_copy == -1) {
    body += "  Error duplicating directory fd.\n";
    goto out;
  }
  dir = fdopendir(dirfd_copy); /* adopts dirfd_copy */
  if (dir == NULL) {
    close(dirfd_copy);
    body += "  Error opening directory.\n";
    goto out;
  }
  rewinddir(dir);
  while (readdir_r(dir, &dent_struct, &dent) == 0 && dent != NULL) {
    size_t name_len = strlen(dent->d_name);
    if (name_len >= 4 && strcmp(dent->d_name + name_len - 4, ".p2p") == 0) {
      files.push_back(string(dent->d_name, name_len - 4));
    }
  }
  closedir(dir);

  std::sort(files.begin(), files.end());
  body += "    <ul>\n";
  for (auto const& file : files) {
    body += "      <li><a href=\"";
    body += file;
    body += "\">";
    body += file;
    body += "</a></li>\n";
  }
  body += "    </ul>\n";

out:
  body += "    <hr>\n";
  body += "    <i>";
  body += PACKAGE_STRING;
  body += "</i>\n";
  body += "  </body>\n";
  body += "</html>\n";

  return body;
}

// Attempt to parse |range_str| as a "ranges-specifier" as defined in
// section 14.35 of RFC 2616. This is typically used in the "Range"
// header of HTTP requests. See
//
//  http://tools.ietf.org/html/rfc2616#section-14.35
//
// NOTE: To keep things simpler, we deliberately do _not_ support the
// full byte range specification.
static bool ParseRange(const string& range_str,
                       uint64_t file_size,
                       uint64_t* range_start,
                       uint64_t* range_end) {
  const char* s = range_str.c_str();

  CHECK(range_start != NULL);
  CHECK(range_end != NULL);

  if (sscanf(s, "bytes=%" SCNu64 "-%" SCNu64, range_start, range_end) == 2 &&
      *range_start <= *range_end) {
    return true;
  } else if (sscanf(s, "bytes=%" SCNu64 "-", range_start) == 1 &&
             *range_start <= file_size - 1) {
    *range_end = file_size - 1;
    return true;
  }

  return false;
}

bool ConnectionDelegate::SendFile(int file_fd, size_t num_bytes_to_send) {
  Time time_start;
  size_t num_total_sent = 0;
  char buf[kPayloadBufferSize];

  num_total_sent = 0;
  time_start = Time::Now();
  while (num_total_sent < num_bytes_to_send) {
    size_t num_to_read = std::min(sizeof buf,
                                  num_bytes_to_send - num_total_sent);
    size_t num_to_send_from_buf;
    size_t num_sent_from_buf;
    ssize_t num_read;

    num_read = read(file_fd, buf, num_to_read);
    if (num_read == 0) {
      // EOF - handle this by sleeping and trying again later
      VLOG(1) << "Got EOF so sleeping one second";
      sleep(1);

      // Give up if socket is no longer connected
      if (IsStillConnected()) {
        continue;
      } else {
        LOG(INFO) << pretty_addr_ << " - peer no longer connected; giving up";
        return false;
      }
    } else if (num_read < 0) {
      // Note that the file is expected to be on a filesystem so Linux
      // guarantees that we never get EAGAIN. In other words, we never
      // get partial reads e.g. either we get everything we ask for or
      // none of it.
      LOG(ERROR) << "Error reading: " << strerror(errno);
      return false;
    }

    num_to_send_from_buf = num_read;
    num_sent_from_buf = 0;
    while (num_to_send_from_buf > 0) {
      ssize_t num_sent =
          send(fd_, buf + num_sent_from_buf, num_to_send_from_buf, 0);
      if (num_sent == -1) {
        LOG(ERROR) << "Error sending: " << strerror(errno);
        return false;
      }
      CHECK_GT(num_sent, 0);
      num_to_send_from_buf -= num_sent;
      num_sent_from_buf += num_sent;
    }
    num_total_sent += num_sent_from_buf;

    // Limit download speed, if requested
    if (max_download_rate_ != 0) {
      Time time_now = Time::Now();
      TimeDelta delta = time_now - time_start;
      uint64_t bytes_allowed = max_download_rate_ * delta.InSecondsF();

      // If we've sent more than the allowed budget for time until now,
      // sleep until this is in the budget
      if (num_total_sent > bytes_allowed) {
        uint64_t over_budget = num_total_sent - bytes_allowed;
        double seconds_to_sleep = over_budget / double(max_download_rate_);
        g_usleep(seconds_to_sleep * G_USEC_PER_SEC);
      }
    }
  }

  // If we served a file, log the time it took us
  if (num_total_sent > 0 && !time_start.is_null()) {
    Time time_end = Time::Now();
    double time_delta = (time_end - time_start).InSecondsF();
    LOG(INFO) << pretty_addr_ << " - sent " << num_total_sent
              << " bytes of response body in " << std::fixed
              << std::setprecision(3) << time_delta << " seconds"
              << " (" << (num_total_sent / time_delta / 1e6) << " MB/s)";
  }

  return true;
}

void ConnectionDelegate::ServiceHttpRequest(
    const string& method,
    const string& uri,
    const string& version,
    const map<string, string>& headers) {
  struct stat statbuf;
  size_t file_size = 0;
  map<string, string> response_headers;
  uint64_t range_first, range_last, range_len;
  uint response_code;
  const char* response_string;
  map<string, string>::const_iterator header_it;
  string file_name;
  int file_fd = -1;
  char ea_value[64] = { 0 };
  ssize_t ea_size;

  // Log User-Agent, if available
  header_it = headers.find("user-agent");
  if (header_it != headers.end()) {
    LOG(INFO) << pretty_addr_ << " - user agent: " << header_it->second;
  }

  if (!(method == "GET" || method == "POST")) {
    SendSimpleResponse(501, "Method Not Implemented");
    goto out;
  }

  // Ensure the URI contains exactly one '/'
  if (uri[0] != '/' || uri.find('/', 1) != string::npos) {
    SendSimpleResponse(400, "Bad Request");
    goto out;
  }

  LOG(INFO) << pretty_addr_ << " - requesting resource with URI " << uri;

  // Handle /index.html
  if (uri == "/" || uri == "/index.html") {
    response_headers["Content-Type"] = "text/html; charset=utf-8";
    SendResponse(200, "OK", response_headers, GenerateIndexDotHtml());
    goto out;
  }

  file_name = uri.substr(1) + ".p2p";
  VLOG(1) << "Opening `" << file_name << "'";
  file_fd = openat(dirfd_, file_name.c_str(), O_RDONLY);
  if (file_fd == -1) {
    SendSimpleResponse(404, string("Error opening file: ") + strerror(errno));
    goto out;
  }

  if (fstat(file_fd, &statbuf) != 0) {
    SendSimpleResponse(404, "Error getting information about file");
    goto out;
  }
  file_size = statbuf.st_size;
  VLOG(1) << "File is " << file_size << " bytes";
  LOG(INFO) << "File is " << file_size << " bytes";

  ea_size =
      fgetxattr(file_fd, "user.cros-p2p-filesize", &ea_value, sizeof ea_value);
  if (ea_size > 0 && ea_value[0] != 0) {
    char* endp = NULL;
    long long int val = strtoll(ea_value, &endp, 0);
    if (endp != NULL && *endp == '\0') {
      VLOG(1) << "Read user.cros-p2p-filesize=" << val;
      if ((size_t) val > file_size) {
        // Simply update file_size to what the EA says - code below
        // handles that by checking for EOF and sleeping
        file_size = val;
      }
    }
  }

  if (file_size == 0) {
    range_first = 0;
    range_last = 0;
    range_len = 0;
    response_code = 200;
    response_string = "OK";
  } else {
    header_it = headers.find("range");
    if (header_it != headers.end()) {
      if (!ParseRange(
              header_it->second, file_size, &range_first, &range_last)) {
        SendSimpleResponse(400, "Error parsing Range header");
        goto out;
      }
      if (range_last >= file_size) {
        SendSimpleResponse(416, "Requested Range Not Satisfiable");
        goto out;
      }
      response_code = 206;
      response_string = "Partial Content";
      response_headers["Content-Range"] =
          std::to_string(range_first) + "-" + std::to_string(range_last) + "/" +
          std::to_string(file_size);
    } else {
      range_first = 0;
      range_last = file_size - 1;
      response_code = 200;
      response_string = "OK";
    }
    CHECK(range_first <= range_last);
    CHECK(range_last < file_size);
    range_len = range_last - range_first + 1;
  }

  response_headers["Content-Type"] = "application/octet-stream";
  response_headers["Content-Length"] = std::to_string(range_len);
  if (!SendResponse(response_code, response_string, response_headers, "")) {
    goto out;
  }

  if (range_first > 0) {
    if (lseek(file_fd, (off_t) range_first, SEEK_SET) != (off_t) range_first) {
      LOG(ERROR) << "Error seeking: " << strerror(errno);
      goto out;
    }
  }

  if (!SendFile(file_fd, range_len))
    goto out;

out:
  if (file_fd != -1)
    close(file_fd);
}

bool ConnectionDelegate::IsStillConnected() {
  char buf[1];
  ssize_t num_recv;

  // Sockets become readable when closed by the peer, which can be
  // used to figure out if the other end is still connected
  num_recv = recv(fd_, buf, 0, MSG_DONTWAIT | MSG_PEEK);
  if (num_recv == -1)
    return true;
  return false;
}

}  // namespace http_server

}  // namespace p2p
