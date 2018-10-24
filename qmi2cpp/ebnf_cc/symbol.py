# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for the representation of symbols.

ebnf_cc parse trees are composed of BaseSymbol nodes. Each BaseSymbol node is a
parse subtree, in which leaf nodes are exclusively of type Token. BaseSymbol
child classes represent various syntactical constructs of the target language,
which corresponds to EBNF rules, and a populated parse tree is simply an
instantiation of the top-level BaseSymbol child class.
"""

import logging


class SymbolException(Exception):
    """Exception class representing an unexpected symbol or lack of symbol."""
    pass


class BaseSymbol(object):
    """Base symbol class.

    Provides the methods needed to consume tokens from the token list. The
    following methods can be placed in the __init__ methods of child classes in
    order to correctly parse the symbol:
      * add
      * add_optional
      * add_token
      * add_token_optional
    """
    _stream = None
    _depth = 0
    _LOG_TAB = '  '

    def __init__(self):
        self._children = []

    @staticmethod
    def set_token_stream(stream):
        """Sets the token stream to read tokens from.

        Note that this method may be called from child classes, and that all
        child classes will read tokens from the same stream. This allows for a
        very readable symbol specification, which makes the generated parser
        code easier to debug.
        """
        BaseSymbol._stream = stream

    def add(self, symbol_class):
        """Adds a child symbol of type symbol_class.

        Will throw a SymbolException if the tokens in the token stream cannot
        be parsed to form a symbol_class.
        """
        self._children.append(symbol_class())

    def add_optional(self, symbol_class):
        """Attempts to add a child symbol of type symbol_class.

        Note: The current implementation does NOT backtrack the token stream
        when an unexpected token is found. Thus parsing will not work in the
        case where some tokens are pulled from the token string prior to finding
        an unexpected token. Most EBNF specifications can be modified to avoid
        this scenario, but future work should fix this behavior.

        Returns:
          True if the symbol was added. False otherwise.
        """
        try:
            self.add(symbol_class)
        except SymbolException:
            # TODO(crbug.com/917913) Add token backtracking functionality
            return False
        return True

    def add_token(self, token_str):
        """Pulls token_str from the token stream and add as a child symbol.

        Raises:
          SymbolException: token stream has been exhausted, or the next token in
              the token stream does not match token_str.
        """
        self._children.append(Token(token_str))
        self._advance_token()

    def add_token_optional(self, token_str):
        """Attempts to add token_str as a child symbol.

        Returns:
          True if the next token in the token stream is token_str. Else False.
        """
        try:
            self.add_token(token_str)
        except SymbolException:
            return False
        return True

    @staticmethod
    def _current_token():
        token = BaseSymbol._stream.current_token()
        if token is None:
            raise SymbolException('Expected a token, but the token stream was '
                                  'exhausted.')
        return token

    @staticmethod
    def _advance_token():
        return BaseSymbol._stream.advance_token()

    @staticmethod
    def _push_depth():
        """Adds indent to subsequent line insertions."""
        BaseSymbol._depth += 1

    @staticmethod
    def _pop_depth():
        """Removes indent from subsequent line insertions."""
        BaseSymbol._depth -= 1

    @staticmethod
    def _get_depth():
        """Gets current indentation of file."""
        return BaseSymbol._depth

    def __repr__(self):
        """Gets string representation of sub-parse tree node."""
        s = '%s:\n' % type(self).__name__
        for child in self._children:
            child_str = '%s' % child
            s += '\n'.join([
                '%s%s' % (self._LOG_TAB, x) for x in child_str.splitlines()
            ]) + '\n'
        return s


class Token(BaseSymbol):
    """Symbol class representing a specific token."""

    def __init__(self, token_str):
        if self._current_token() != token_str:
            raise SymbolException('Expected token "%s", but found "%s".' %
                                  (token_str, self._current_token()))
        self._str = token_str

    def __repr__(self):
        return 'Token(%s)' % self._str
