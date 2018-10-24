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
        elif self.add_optional(Option_Start):
            self.add(Rhs)
            self.add(Option_End)
        elif self.add_optional(Repeat_Start):
            self.add(Rhs)
            self.add(Repeat_End)
        elif self.add_optional(Group_Start):
            self.add(Rhs)
            self.add(Group_End)
        else:
            raise symbol.SymbolException(
                'Unexpected token "%s"' % self._current_token())

        if self.add_optional(Concat):
            self.add(Rhs)
        elif self.add_optional(Or):
            self.add(Rhs)

        self._pop_depth()
        logging.debug('%sFinished adding Rhs.',
                      (self._get_depth() * self._LOG_TAB))


class Elementary_Symbol(symbol.BaseSymbol):
    """
    Parse tree node representing any elementary EBNF symbol.

    An elementary symbol is either an output symbol (a terminal or a
    non-terminal identifier) or an EBNF operator. This class exists primarily
    for ease of parse tree traversal.
    """
    pass


class Output(Elementary_Symbol):
    """Parse tree node representing an output symbol."""
    pass


class Operator(Elementary_Symbol):
    """Parse tree node representing an EBNF operator."""
    pass


class StartOperator(Operator):
    """Operator node representing a start symbol for multi-symbol operators."""
    pass


class EndOperator(Operator):
    """Operator node representing an end symbol for multi-symbol operators."""
    pass


class Terminal(Output):
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


class Identifier(Output):
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


class Concat(Operator):
    """Parse tree node representing a concatenation operator."""

    def __init__(self):
        super(Concat, self).__init__()
        self.add_token(',')


class Or(Operator):
    """Parse tree node representing an or operator."""

    def __init__(self):
        super(Or, self).__init__()
        self.add_token('|')


class Option_Start(StartOperator):
    """Parse tree node representing the start of an optional section."""

    def __init__(self):
        super(Option_Start, self).__init__()
        self.add_token('[')


class Option_End(EndOperator):
    """Parse tree node representing the end of an optional section."""
    def __init__(self):
        super(Option_End, self).__init__()
        self.add_token(']')


class Repeat_Start(StartOperator):
    """Parse tree node representing the start of a repeat section."""

    def __init__(self):
        super(Repeat_Start, self).__init__()
        self.add_token('{')


class Repeat_End(EndOperator):
    """Parse tree node representing the end of a repeat section."""

    def __init__(self):
        super(Repeat_End, self).__init__()
        self.add_token('}')


class Group_Start(StartOperator):
    """Parse tree node representing the start of a grouped section."""

    def __init__(self):
        super(Group_Start, self).__init__()
        self.add_token('(')


class Group_End(EndOperator):
    """Parse tree node representing the end of a grouped section."""

    def __init__(self):
        super(Group_End, self).__init__()
        self.add_token(')')


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
