# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module providing functionality for parser generation."""

import logging
import re

from ebnf_cc import ebnf_parser
from ebnf_cc import token_stream


class ParserGenerator(object):
    """Class responsible for the generation of parser given an EBNF file."""

    def __init__(self, ebnf_filename):
        """Creates a ParserGenerator object.

        Args:
          ebnf_filename: str name of the EBNF file containing the
              language specification.
        """
        self._ebnf_filename = ebnf_filename
        self._token_stream = None
        self._index = None

    def produce_parser(self, output_filename):
        """Top-level parser generation method.

        Parses the provided EBNF file and generates a recursive-descent parser
        matching the specified language.

        Args:
          output_filename: str name of the file to output the parser to.
        """
        with open(self._ebnf_filename) as f:
            # Split text into tokens, where ' and " should be their own tokens.
            token_list = re.sub(r'([\"\'])', ' \\1 ', f.read()).split()
            self._token_stream = token_stream.TokenStream(token_list)
        self._index = 0

        # Parse EBNF file into Grammar object (which is the top-level
        # non-terminal in the EBNF specification of EBNF).
        ebnf_parser.Grammar.set_token_stream(self._token_stream)
        grammar = ebnf_parser.Grammar()
        logging.debug(grammar)

        # TODO(akhouderchah) Create parser by traversing the Grammar object.
