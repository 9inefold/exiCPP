import json, os, sys, subprocess, traceback, itertools
from glob import glob
from pathlib import Path
#from subprocess import run as run_proc
from exiconf.constants import *
from exiconf.cl_args import parse_args
from exiconf.logging import errs, outs
from exiconf.test.cache import ProcessCache

__all__ = ['main']

#OUT_DIR = TEST_OUT_DIR
OUT_DIR = TEST_OUT_DIR2

FOLDER_NAMES = [
  # Exicpp, OpenEXI, Exificient encode
  'i', 'o', #'x',
  # Exicpp roundtrip and decode
  'ii', 'io', #'ix',
  # OpenEXI roundtrip and decode
  'oo', 'oi', #'ox',
  # Exificient roundtrip and decode
  #'xx', 'xo', 'xi',
]

# Loads file with glob pattern
def _load_glob_recursive(pattern: str, root, sort=False, strip_ext=True) -> list[str]:
  all_files = glob(
    pattern,
    root_dir=root,
    recursive=True
  )
  # Convert to posix form
  if strip_ext:
    out = [Path(f).with_suffix('').as_posix() for f in all_files]
  else:
    out = [Path(f).as_posix() for f in all_files]
  # Sort if needed
  if sort:
    out.sort()
  return out

# Loads file with glob pattern
def _load_ext_recursive(extension: str, root, **kwargs) -> list[str]:
  return _load_glob_recursive(f'**/*.{extension}', root, **kwargs)

# Gets all the needed entries
def _get_entries(root=None) -> list[tuple[str, str, bool]]:
  if root is None:
    root = TEST_SRC_DIR
  # Get files of each type
  xml_files = _load_ext_recursive('xml', root, sort=True)
  ent_files = set(_load_ext_recursive('ent', root))
  # Make a new list
  return [(f.replace('/', '.'), Path(f + '.xml'), f in ent_files) for f in xml_files]

# The default program entry point
def main():
  #parser = get_arg_parser()
  #print(parser.format_help(), flush=True)
  args = parse_args()

  xml_files = _get_entries(TEST_SRC_DIR)
  #print([f[0] for f in xml_files])

  ALIGNMENT = ['i', 'y']
  #PRESERVE = ['c', 'di', 'l', 'p']
  PRESERVE = ['c', 'di', 'p']

  preserve = ['']
  for n in range(1, len(PRESERVE) + 1):
    it = itertools.combinations(PRESERVE, n)
    preserve.extend(['P' + ''.join(c) for c in it])
  print(preserve)

  with ProcessCache(args.cachefile) as cache:
    name, path, has_ent = xml_files[0]
    entry0 = cache.get(name, path)

    #results = entry0.get('yPcdip')
    #results.passed('i')
    #results.failed('o')

    #cache.clear('at.at-*/*P*')

    # ...
    pass
