import argparse, shlex
import os, sys
from exiconf.constants import __version__
from exiconf.logging import LogLevelArgs
from exiconf.version_tuple import version_tuple

__all__ = ['parse_args']
LOG_LEVELS = '{' + ', '.join(LogLevelArgs.MAP_LEVELS) + '}'

# From argparse
def _copy_items(items):
  if items is None:
    return []
  # The copy module is used only in the 'append' and 'append_const'
  # actions, and it is needed only when the default value isn't a list.
  # Delay its import for speeding up the common case.
  if type(items) is list:
    return items[:]
  import copy
  return copy.copy(items)

# Class for parsing jvm diagnostic levels
class JVMLogLevelAction(argparse.Action):
  def __init__(self, option_strings, dest, nargs=None, **kwargs):
    super().__init__(option_strings, dest, **kwargs)

  def __call__(self, parser, namespace, value, option_string=None):
    items = getattr(namespace, self.dest, None)
    items = _copy_items(items)
    items.append(f'-Dexicpp.loglevel={loglevel}')
    setattr(namespace, self.dest, items)

# Gets the parameters passed to the parser init
def _get_parser_params():
  out = {}
  # Get the library argparse version
  pyver = version_tuple(sys.version_info[0:3])
  if pyver > version_tuple('3.14'):
    out['suggest_on_error'] = True
  return out

# Creates the parser
def get_arg_parser() -> argparse.ArgumentParser:
  parser_params = _get_parser_params()
  parser = argparse.ArgumentParser(prog="exiconf", **parser_params)
  #parser = argparse.ArgumentParser(prog="exiconf", suggest_on_error=True)
  parser.add_argument(
    '--version', action='version',
    version="%(prog)s " + __version__
  )

  parser.add_argument(
    '-r', '--restrict', '--restrict-to',
    dest='restrict',
    action='extend',
    nargs='*',
    type=str, default=[],
    help='restrict to specific encodings'
  )
  parser.add_argument(
    '-c', '--clear', '--clear-cache',
    dest='clear',
    action='extend',
    nargs='*',
    type=str,
    help='clears cache (or specific entries)'
  )

  # TODO: Add --extended-checks for chaining (eg. openexi -> exicpp -> exicpp -> exificient).

  parser.add_argument(
    '--diagnostic-level',
    dest='diag_level',
    help="control how verbose exiconf should be (default: info)",
    choices=LogLevelArgs.ALL_LEVELS,
    metavar=LOG_LEVELS,
    default='info'
  )
  parser.add_argument(
    '-d', '--diag',
    dest='diag_level',
    help="alias for '--diagnostic-level=LEVEL'",
    choices=LogLevelArgs.ALL_LEVELS,
    metavar='LEVEL',
    default='info'
  )
  parser.add_argument(
    '-q', '--quiet',
    dest='diag_level',
    action='store_const',
    help="alias for '--diagnostic-level=error'",
    const='error'
  )
  parser.add_argument(
    '-v', '--verbose',
    action='store_const',
    help="alias for '--diagnostic-level=verbose'",
    const='verbose'
  )
  parser.add_argument(
    '--color', '--use-color',
    dest='color',
    action='store_true',
    help='print with color',
    default=False
  )

  parser.add_argument(
    '--jvm', '--jvm-path',
    dest='jvm_path',
    action='store',
    type=str, default=None,
    help='set the path to a specific JVM version'
  )
  parser.add_argument(
    '--jvm-classpath',
    dest='jvm_classpath',
    action='extend',
    nargs='*',
    type=str, default=[],
    help='pass extra JVM class paths in the glob format'
  )
  parser.add_argument(
    '-ea', '--jvm-assert', '--jvm-assertions',
    dest='jvm_args',
    action='append_const',
    help='enable JVM assertions',
    const='-ea'
  )
  parser.add_argument(
    '--jvm-diag', '--jvm-diagnostic-level',
    dest='jvm_diag_level',
    action=JVMLogLevelAction,
    help="control how verbose java should be (default: error)",
    choices=LogLevelArgs.ALL_LEVELS,
    metavar='LEVEL',
    default='error'
  )

  return parser

# Parses arguments from the command line
def parse_args(env_override=True):
  if env_override:
    # Environment variables can override command line arguments by default.
    env_args = shlex.split(os.environ.get("EXICONF_OPTS", ""))
    args = sys.argv[1:] + env_args
  else:
    # Don't override
    # TODO: Improve this?
    args = sys.argv[1:]
  parser = get_arg_parser()
  return parser.parse_args(args)
