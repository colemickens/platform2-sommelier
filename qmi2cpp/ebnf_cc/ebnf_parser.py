# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the functionality to parse EBNF specifications."""

import logging

from ebnf_cc import symbol


class Grammar(symbol.BaseSymbol):
    """Parse tree node representing a complete EBNF grammar."""

    def __init__(self):
        super(Grammar, self).__init__()
        logging.debug('%sAdding Grammar...',
                      (self._get_depth() * self._LOG_TAB))
        self._push_depth()
        while True:
            if not self.add_optional(Rule):
                break
        self._pop_depth()
        logging.debug('%sFinished adding Grammar.',
                      (self._get_depth() * self._LOG_TAB))


class Rule(symbol.BaseSymbol):
    """Parse tree node representing a top-level EBNF rule."""

    def __init__(self):
        super(Rule, self).__init__()
        logging.debug('%sAdding Rule...', (self._get_depth() * self._LOG_TAB))
        self._push_depth()
        self.add(Lhs)
        self.add_token('=')
        self.add(Rhs)
        self.add_token(';')
        self._pop_depth()
        logging.debug('%sFinished adding Rule.',
                      (self._get_depth() * self._LOG_TAB))


class Lhs(symbol.BaseSymbol):
    """Parse tree node representing the left-hand side of an EBNF rule."""

    def __init__(self):
        super(Lhs, self).__init__()
        logging.debug('%sAdding Lhs...', (self._get_depth() * self._LOG_TAB))
        self._push_depth()
        self.add(Identifier)
        self._pop_depth()
        logging.debug('%sFinished adding Lhs.',
                      (self._get_depth() * self._LOG_TAB))


class Rhs(symbol.BaseSymbol):
    """Parse tree node representing the right-hand side of an EBNF rule."""

    def __init__(self):
        super(Rhs, self).__init__()
        logging.debug('%sAdding Rhs...', (self._get_depth() * self._LOG_TAB))
        self._push_depth()
        if self.add_optional(Identifier):
            pass
        elif self.add_optional(Terminal):
            pass
        elif self.add_token_optional('['):
            self.add(Rhs)
            self.add_token(']')
        elif self.add_token_optional('{'):
            self.add(Rhs)
            self.add_token('}')
        elif self.add_token_optional('('):
            self.add(Rhs)
            self.add_token(')')
        else:
            raise symbol.SymbolException(
                'Unexpected token "%s"' % self._current_token())

        if self.add_token_optional('|'):
            self.add(Rhs)
        elif self.add_token_optional(','):
            self.add(Rhs)

        self._pop_depth()
        logging.debug('%sFinished adding Rhs.',
                      (self._get_depth() * self._LOG_TAB))


class Terminal(symbol.BaseSymbol):
    """Parse tree node representing a terminal within an EBNF rule."""

    def __init__(self):
        super(Terminal, self).__init__()
        logging.debug('%sAdding Terminal...',
                      (self._get_depth() * self._LOG_TAB))
        self._push_depth()
        if self.add_token_optional("'"):
            self.add(EbnfToken)
            self.add_token("'")
        elif self.add_token_optional('"'):
            self.add(EbnfToken)
            self.add_token('"')
        else:
            raise symbol.SymbolException(
                'Expected a \' or \", but instead '
                'received "%s".' % self._current_token())
        self._pop_depth()
        logging.debug('%sFinished adding Terminal.',
                      (self._get_depth() * self._LOG_TAB))


class Identifier(symbol.BaseSymbol):
    """Parse tree node representing a identifier within an EBNF rule."""

    def __init__(self):
        super(Identifier, self).__init__()
        logging.debug('%sAdding Identifier...',
                      (self._get_depth() * self._LOG_TAB))
        self._push_depth()
        curr_token = self._current_token()
        if not curr_token:
            raise symbol.SymbolException('Expected a nonempty token')
        if not curr_token[0].isalpha():
            raise symbol.SymbolException(
                'Expected an identifier, which must '
                'begin with a letter. Instead received "%s".' % curr_token)
        self.add_token(curr_token)
        self._pop_depth()
        logging.debug('%sFinished adding Identifier.',
                      (self._get_depth() * self._LOG_TAB))


class EbnfToken(symbol.BaseSymbol):
    """Parse tree node representing any token."""

    def __init__(self):
        super(EbnfToken, self).__init__()
        logging.debug('%sAdding EBNF Token...',
                      (self._get_depth() * self._LOG_TAB))
        self._push_depth()
        self.add_token(self._current_token())
        self._pop_depth()
        logging.debug('%sFinished adding EBNF Token.',
                      (self._get_depth() * self._LOG_TAB))
