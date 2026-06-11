import os, sys, shutil
import json, subprocess, traceback, functools
from glob import glob
from pathlib import Path
#from subprocess import run as run_proc
from exiconf.constants import *
from exiconf.cl_args import parse_args
from exiconf.logging import errs, outs, set_log_level, enable_color
from exiconf.jvm.setup import start_jvm
from exiconf.test.cache import ProcessCache, ProcessCacheEntry
from exiconf.test.mapfile import map_files, MappingDataEntry

from exiconf.exicpp_coder import ExicppCoder
from exiconf.openexi_coder import OpenEXICoder
# TODO: Start up JVM

__all__ = ['main']

#OUT_DIR = TEST_OUT_DIR
OUT_DIR = TEST_OUT_DIR2

BASE_FOLDER_NAMES = [
  # Exicpp, OpenEXI, Exificient encode
  'i', 'o', #'x',
]
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

def handle_clear(cache: ProcessCache, clear: list[str]):
  if len(clear) == 0:
    cache.clear()
  else:
    for to_clear in clear:
      cache.clear(to_clear)

@functools.cache
def get_coder(val: str, mangled: str):
  match val:
    case 'i':
      cls = ExicppCoder
    case 'o':
      cls = OpenEXICoder
    case _:
      raise ValueError(f"expected {BASE_FOLDER_NAMES}, got '{val}'")
  # Create new instance
  return cls(mangled)

def run_tests(name: str, data: MappingDataEntry, entry: ProcessCacheEntry):
  outpath = OUT_DIR / name
  if not outpath.exists():
    outpath.mkdir(parents=True)
  # Copy all our dependencies
  if data.dependencies is not None:
    for dep in data.dependencies:
      lnk = outpath / dep.name
      #os.symlink(dep, lnk)
      shutil.copy(dep, lnk)
  
  for test in data.tests:
    coder = get_coder('i', test)
    res = entry.get(test)
    # TODO: Actually do stuff
    pass
  pass

# The default program entry point
def main():
  args = parse_args()
  set_log_level(args.diag_level)
  enable_color(args.color)

  start_jvm(
    jvm_path=args.jvm_path,
    classpath=args.jvm_classpath,
    jvm_args=args.jvm_args
  )

  file_map = map_files(root=args.root)
  with ProcessCache(args.cachefile) as cache:
    if args.clear is not None:
      handle_clear(cache, args.clear)
    # Run the actual tests
    for name, data in file_map.items():
      entry = cache.get(name, data.file)
      #if entry.did_all_pass():
      #  continue
      run_tests(name, data, entry)
