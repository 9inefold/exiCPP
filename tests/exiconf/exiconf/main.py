import json, os, sys, subprocess, traceback
from glob import glob
from pathlib import Path
#from subprocess import run as run_proc
from exiconf.constants import *
from exiconf.cl_args import parse_args
from exiconf.logging import errs, outs
from exiconf.test.cache import ProcessCache
from exiconf.test.mapfile import map_files

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

# The default program entry point
def main():
  #parser = get_arg_parser()
  #print(parser.format_help(), flush=True)
  args = parse_args()
  file_map = map_files(root=args.root)
  #for name, data in file_map.items():
  #  print(name, data.dependencies)

  with ProcessCache(args.cachefile) as cache:
    for name, data in file_map.items():
      entry = cache.get(name, data.file)
      

      #results = entry.get('yPcdip')
      #results.passed('i')
      #results.failed('o')

      #cache.clear('at.at-*/*P*')

      # ...
      pass
