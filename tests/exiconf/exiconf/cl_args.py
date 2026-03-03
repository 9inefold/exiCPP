import argparse, shlex
import os, sys
from exiconf.constants import __version__
from exiconf.logging import LogLevelArgs

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

class JVMLogLevelAction(argparse.Action):
  def __init__(self, option_strings, dest, nargs=None, **kwargs):
    super().__init__(option_strings, dest, **kwargs)

  def __call__(self, parser, namespace, value, option_string=None):
    items = getattr(namespace, self.dest, None)
    items = _copy_items(items)
    items.append(f'-Dexicpp.loglevel={loglevel}')
    setattr(namespace, self.dest, items)

def parse_args():
  parser = argparse.ArgumentParser(prog="exiconf")
  parser.add_argument(
    '--version', action='version',
    version="%(prog)s " + __version__
  )

  parser.add_argument(
    '--diag', '--diagnostic-level',
    dest='diag_level',
    help="control how verbose exiconf should be (default info)",
    choices=LogLevelArgs.ALL_LEVELS,
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
    '--verbose',
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
    help="control how verbose java should be (default error)",
    choices=LogLevelArgs.ALL_LEVELS,
    default='error'
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
    '--clear', '--clear-cache',
    dest='clear',
    action='extend',
    nargs='*',
    type=str,
    help='clears cache (or specific entries)'
  )

  # LIT is special: environment variables override command line arguments.
  env_args = shlex.split(os.environ.get("EXICONF_OPTS", ""))
  args = sys.argv[1:] + env_args
  return parser.parse_args(args)
