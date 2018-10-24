# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ebnf_cc is a parser-generator for languages described in EBNF.

A provided EBNF specification of a languange will be used to generate a
recursive-descent parser in Python for the described language.
"""

import argparse
import logging

from ebnf_cc import parser_generator


VERBOSITY_LEVELS = [logging.CRITICAL,
                    logging.ERROR,
                    logging.WARNING,
                    logging.INFO,
                    logging.DEBUG]


def get_opts(argv):
    """Parses arguments from command line.

    Returns:
      argparse.Namespace object representing the command line arguments.
    """
    def valid_verbosity(verbosity):
        verbosity = int(verbosity)
        if 0 <= verbosity < len(VERBOSITY_LEVELS):
            return verbosity
        raise argparse.ArgumentTypeError(
            '%s is not a valid verbosity level' % verbosity)

    parser = argparse.ArgumentParser(
        description=__doc__)
    parser.add_argument(
        'ebnf_filename',
        type=str,
        help='Filename of the language\'s EBNF specification.')
    parser.add_argument(
        '-v', '--verbosity',
        type=valid_verbosity,
        default=3,
        help='Set the verbosity level of logging, from 0 (only log critical '
        'errors) to 4 (log everything). Default is 3.')

    return parser.parse_args()


def main(argv):
    if len(argv) < 2:
        return 1
    opts = get_opts(argv)

    logging.basicConfig(level=VERBOSITY_LEVELS[opts.verbosity])

    generator = parser_generator.ParserGenerator(opts.ebnf_filename)
    generator.produce_parser('parser.py')
    return 0
