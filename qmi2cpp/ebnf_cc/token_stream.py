# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to represent tokens as a stream."""

import logging


class TokenStream(object):
    """Class representing a stream of tokens.

    Used by the symbol.BaseSymbol classes, which consume tokens from this
    stream when created.
    """

    def __init__(self, token_list):
        self._tokens = token_list
        self._index = 0

    def current_token(self):
        if not self.is_empty():
            logging.debug('current_token called: "%s"',
                          self._tokens[self._index])
            return self._tokens[self._index]
        return None

    def advance_token(self):
        self._index += 1

    def is_empty(self):
        return len(self._tokens) <= self._index
