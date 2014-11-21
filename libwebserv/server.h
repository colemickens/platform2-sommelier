// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEBSERV_SERVER_H_
#define LIBWEBSERV_SERVER_H_

#include <map>
#include <memory>
#include <string>

#include <base/callback_forward.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/strings/string_piece.h>

#include "libwebserv/export.h"
#include "libwebserv/request_handler_interface.h"

struct MHD_Daemon;

namespace base {
class TaskRunner;
}  // namespace base

namespace libwebserv {

// Top-level wrapper class around HTTP server and provides an interface to
// the web server. It allows the users to start the server and
// register request handlers.
class LIBWEBSERV_EXPORT Server final {
 public:
  Server();
  ~Server();

  // Starts the server and makes it listen to requests on the given port.
  bool Start(uint16_t port);

  // Stops the server.
  bool Stop();

  // Adds a request handler for given |url|. If the |url| ends with a '/', this
  // makes the handler respond to any URL beneath this path.
  // Note that it is not possible to add a specific handler just for the root
  // path "/". Doing so means "respond to any URL".
  // |method| is optional request method verb, such as "GET" or "POST".
  // If |method| is empty, the handler responds to any request verb.
  // If there are more than one handler for a given request, the most specific
  // match is chosen. For example, if there are the following handlers provided:
  //    - A["/foo/",  ""]
  //    - B["/foo/bar", "GET"]
  //    - C["/foo/bar", ""]
  // Here is what handlers are called when making certain requests:
  //    - GET("/foo/bar")   => B[]
  //    - POST("/foo/bar")  => C[]
  //    - PUT("/foo/bar")   => C[]
  //    - GET("/foo/baz")   => A[]
  //    - GET("/foo")       => 404 Not Found
  // This functions returns a handler ID which can be used later to remove
  // the handler.
  int AddHandler(const base::StringPiece& url,
                 const base::StringPiece& method,
                 std::unique_ptr<RequestHandlerInterface> handler);

  // Similar to AddHandler() above but the handler is just a callback function.
  int AddHandlerCallback(
      const base::StringPiece& url,
      const base::StringPiece& method,
      const base::Callback<RequestHandlerInterface::HandlerSignature>&
          handler_callback);

  // Removes the handler with the specified |handler_id|.
  // Returns false if the handler with the given ID is not found.
  bool RemoveHandler(int handler_id);

  // Finds the handler ID given the exact match criteria. Note that using
  // this function could cause unexpected side effects if there are more than
  // one handler registered for given URL/Method parameters.
  // It is better to remember the handler ID from AddHandler() method and use
  // that ID to remove the handler, instead of looking the handler up using
  // URL/Method.
  int GetHandlerId(const base::StringPiece& url,
                   const base::StringPiece& method) const;

  // Finds a handler for given URL/Method. This method does the criteria
  // matching and not exact match performed by GetHandlerId. This is the method
  // used to look up the handler for incoming HTTP requests.
  RequestHandlerInterface* FindHandler(
      const base::StringPiece& url,
      const base::StringPiece& method) const;

 private:
  MHD_Daemon* server_ = nullptr;
  scoped_refptr<base::TaskRunner> task_runner_;

  struct LIBWEBSERV_PRIVATE HandlerMapEntry {
    std::string url;
    std::string method;
    std::unique_ptr<RequestHandlerInterface> handler;
  };

  std::map<int, HandlerMapEntry> request_handlers_;
  int last_handler_id_{0};

  friend class ServerHelper;
  friend class Connection;
  DISALLOW_COPY_AND_ASSIGN(Server);
};

}  // namespace libwebserv

#endif  // LIBWEBSERV_SERVER_H_
