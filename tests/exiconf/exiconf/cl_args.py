import argparse, shlex
import os, sys
from pathlib import Path
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
    items.append(f'-Dexicpp.loglevel={value}')
    setattr(namespace, self.dest, items)

# Gets the parameters passed to the parser init
def _get_parser_params():
  out: dict = {}
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
  parser.add_argument(
    '--cachefile',
    dest='cachefile',
    action='store',
    help='use a different cachefile name',
    type=str,
    default=None
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
    type=_path, default=None,
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
    dest='jvm_args',
    action=JVMLogLevelAction,
    help="control how verbose java should be (default: error)",
    choices=LogLevelArgs.ALL_LEVELS,
    metavar='LEVEL',
    default='error'
  )

  return parser

def _realpath(arg) -> Path:
  out = _path(arg)
  if not out.exists():
    raise _error(f"path '{out.as_posix()}' does not exist")
  return out

def _path(arg) -> Path:
  try:
    return Path(arg)
  except:
    raise _error(f"expected path but got '{arg}'")

def _error(msg):
  return argparse.ArgumentTypeError(msg)

################################################################################

# Gets arguments from a file
def _expand_file_to_args(filename: str) -> list[str]:
  path = Path(filename)
  if not path.exists():
    raise FileNotFoundError('File does not exist!')
  blob = path.read_text(encoding='utf8')
  lines = blob.splitlines()
  # Extract significant lines
  out = []
  for line in lines:
    stripped = line.strip()
    if not stripped.startswith('#'):
      out.append(stripped)
  return out

# Expands arguments from a @FILE argument
def _expand_argv(argv: list[str]) -> list[str]:
  out = []
  for arg in argv:
    if not arg.startswith('@'):
      out.append(arg)
      continue
    # We found one!
    try:
      from_file = _expand_file_to_args(arg[1:])
      out.extend(from_file)
    except Exception as e:
      print(f'Unable to read from {arg}: {e}')
  return out

# Parses arguments from the command line
def parse_args(env_override=True):
  args = _expand_argv(sys.argv[1:])
  if env_override:
    # Environment variables can override command line arguments by default.
    env_args = shlex.split(os.environ.get("EXICONF_OPTS", ""))
    args.extend(env_args)
  #print(' '.join(args))
  parser = get_arg_parser()
  return parser.parse_args(args)
