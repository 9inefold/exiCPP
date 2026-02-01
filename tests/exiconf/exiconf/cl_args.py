import argparse, shlex
import os, sys
from exiconf.constants import __version__

def parse_args():
  parser = argparse.ArgumentParser(prog="exiconf")
  parser.add_argument(
    '--version', action='version',
    version="%(prog)s " + __version__
  )

  parser.add_argument(
    '--diagnostic-level',
    dest='diag_level',
    help="control how verbose exiconf should be (default info)",
    choices=['error', 'info', 'verbose'],
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
