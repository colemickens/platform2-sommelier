This directory contains a web server (webservd) and a client interface library
(libwebserv).

The current implementation of the web server uses libmicrohttpd for actual
HTTP request processing and dispatches the requests to libwebserv clients over
D-Bus.
