# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing functionality to convert an EBNF parse tree into an
intermediate language representation.

Converting the parse tree into an intermediate language representation allows
for proper handling of operator precedence, serves to insulate the parser
generator code from changes to the EBNF language or parse tree structure used.
"""

import logging

from ebnf_cc import ebnf_parser


class _ILObject(object):
    """Base class for IL objects representing EBNF operations."""

    def __init__(self, a, b, is_idempotent):
        """Creates an _ILObject instance with operands a and b.

        Params:
          a: ebnf_parser.BaseSymbol or _ILObject, representing a parse tree
              node or an EBNF operation, respectively. May also be None.
          b: ebnf_parser.BaseSymbol or _ILObject, representing a parse tree
              node or an EBNF operation, respectively. May also be None.
          is_idempotent: whether the specified operation is idempotent (e.g. a
              Choice of a Choice is still just a Choice). This value is set
              only by child classes, not by users of these classes.
        """
        self._objs = []
        if is_idempotent:
            if isinstance(a, self.__class__):
                self._objs = a._objs
            else:
                self._objs.append(a)
            if isinstance(b, self.__class__):
                self._objs.extend(b._objs)
            else:
                self._objs.append(b)
        else:
            if a is not None:
                self._objs.append(a)
            if b is not None:
                self._objs.append(b)

    def __repr__(self):
        s = '%s(' % type(self).__name__
        for obj in self._objs:
            if isinstance(obj, _ILObject):
                s += '%s, ' % obj
            elif isinstance(obj, ebnf_parser.Terminal):
                s += '%s, ' % obj._children[1]._children[0]._str
            else:
                s += '%s, ' % obj._children[0]._str
        return s[:-2] + ')'


class Choice(_ILObject):
    """Choice between different terminals, non-terminals, and _ILObjects.

    The IL representation of the EBNF '|' operator.
    """

    def __init__(self, a, b):
        super(Choice, self).__init__(a, b, True)


class Sequence(_ILObject):
    """Sequence of terminals, non-terminals, and _ILObjects.

    The IL representation of the EBNF ',' operator.
    """

    def __init__(self, a, b):
        super(Sequence, self).__init__(a, b, True)


class Repetition(_ILObject):
    """Repetitions of a sequence of terminals, non-terminals, and _ILObjects.

    The IL representation of an EBNF sequence surrounded with '{' and '}'.
    """

    def __init__(self, a, b):
        super(Repetition, self).__init__(a, b, False)


class Optional(_ILObject):
    """Optional sequence of terminals, non-terminals, and _ILObjects.

    The IL representation of an EBNF sequence surrounded with '[' and ']'.
    """

    def __init__(self, a, b):
        super(Optional, self).__init__(a, b, False)


class ILGenerator(object):
    """Class to take an EBNF parse tree and generate an intermediate language
    representation.

    This implementation currently only accepts subtrees from Rhs or below, as
    there doesn't seem to be much benefit to converting the entire grammar into
    an IL.

    The conversion from parse tree to IL results in the following levels of
    operator precedence from highest to lowest:
        {}[]()
        |
        ,

    The current implementation uses the Shunting-Yard algorithm for parse tree
    evaluation, but the interface does not preclude the use of a different
    solution in the future.
    """

    def generate(self, parse_tree):
        """Top-level method to generate intermediate language representation.

        Params:
          parse_tree: parser.BaseSymbol representing the parse subtree
              to convert into the IL.
        """
        self._output_stack = []
        self._operator_stack = []

        # Add symbols to their appropriate stack, performing any necessary
        # processing in order to maintain the documented operator precedence
        while True:
            next_symbol = parse_tree.get_next(ebnf_parser.Elementary_Symbol)
            if next_symbol is None:
                break
            if isinstance(next_symbol, ebnf_parser.Output):
                self._output_stack.append(next_symbol)
            elif isinstance(next_symbol, ebnf_parser.Operator):
                self._add_operator(next_symbol)
            else:
                raise ValueError('Found symbol that is neither an Output nor '
                                 'Operator.')

        # Flush the output and operator stacks to return a single expression
        while self._pop_operator():
            pass
        if len(self._output_stack) > 1:
            logging.warning('Output stack has more than a single element when '
                            'being returned!')
        return self._output_stack[0]

    def _pop_operator(self):
        """Pops the top operator and pushes the result to the output stack.

        Returns:
          The popped operator if there were operators left on the stack. Else
          None.
        """
        if not self._operator_stack:
            return False

        op = self._operator_stack.pop()
        if (isinstance(op, ebnf_parser.EndOperator) or
            isinstance(op, ebnf_parser.StartOperator)):
            pass
        elif isinstance(op, ebnf_parser.Or):
            b = self._output_stack.pop()
            a = self._output_stack.pop()
            self._output_stack.append(Choice(a, b))
        elif isinstance(op, ebnf_parser.Concat):
            b = self._output_stack.pop()
            a = self._output_stack.pop()
            self._output_stack.append(Sequence(a, b))
        else:
            raise ValueError('Unrecognized Output type')
        return op

    def _add_operator(self, op):
        """Adds operator to the operator stack."""
        if isinstance(op, ebnf_parser.EndOperator):
            while not isinstance(self._pop_operator(),
                                 ebnf_parser.StartOperator):
                pass
            if isinstance(op, ebnf_parser.Option_End):
                a = self._output_stack.pop()
                self._output_stack.append(Optional(a, None))
            elif isinstance(op, ebnf_parser.Repeat_End):
                a = self._output_stack.pop()
                self._output_stack.append(Repetition(a, None))
        elif isinstance(op, ebnf_parser.StartOperator):
            self._operator_stack.append(op)
        elif not self._operator_stack:
            self._operator_stack.append(op)
        else:
            while self._operator_stack:
                prev_op = self._operator_stack[-1]
                # Or is left associative and of greater precedence than Concat,
                # and so will always be popped before another Or or Concat.
                if isinstance(prev_op, ebnf_parser.Or):
                    self._pop_operator()
                else:
                    break
            self._operator_stack.append(op)
