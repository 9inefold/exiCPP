import os, sys, shutil
import json, subprocess, traceback, functools
#from glob import glob
from pathlib import Path
#from subprocess import run as run_proc
from exiconf.constants import *
from exiconf.cl_args import ArgNamespace
from exiconf.logging import errs, outs
from exiconf.test.cache import *
from exiconf.test.mapfile import map_files, MappingDataEntry

from exiconf.exicpp_coder import ExicppCoder
from exiconf.openexi_coder import OpenEXICoder
# from exiconf.exificient_coder import ExificientCoder

__all__ = ['runner_main', 'run_tests']

#OUT_DIR = TEST_OUT_DIR
OUT_DIR = TEST_OUT_DIR2

CODER_NAMES = [
  # Exicpp, OpenEXI, Exificient encode
  'i', 'o', #'x',
]

def handle_clear(cache: ProcessCache, clear: list[str]):
  if len(clear) == 0:
    cache.clear()
  else:
    for to_clear in clear:
      cache.clear(to_clear)

@functools.cache
def get_coder(typ: str, mangled: str):
  match typ:
    case 'i':
      cls = ExicppCoder
    case 'o':
      cls = OpenEXICoder
    case 'x':
      raise NotImplementedError('exificient coder is not implemented yet!')
    case _:
      raise ValueError(f"expected {CODER_NAMES}, got '{typ}'")
  # Create new instance
  return cls(mangled)

def run_individual_test(
    mangled: str, name: str,
    input: Path, outpath: Path,
    results: ProcessCacheResults, /):
  # ...
  encoded = []
  for typ in CODER_NAMES:
    filename = outpath / f'{mangled}.{typ}.exi'
    if results.did_pass(typ) and filename.exists():
      # Skip work if we can
      encoded.append(typ)
      continue
    # Actually run the coder
    coder = get_coder(typ, mangled)
    if coder.encode_file(input, filename):
      encoded.append(typ)

  # TODO: Actually do stuff

  pass

def run_tests(name: str, data: MappingDataEntry, entry: ProcessCacheEntry, /):
  outpath = OUT_DIR / name
  if not outpath.exists():
    outpath.mkdir(parents=True)
  # Make a copy of the file
  # TODO: Hash the file to validate tests?
  input = outpath / data.file.name
  shutil.copy(data.file, input)
  
  # Copy all our dependencies
  if data.dependencies is not None:
    for dep in data.dependencies:
      lnk = outpath / dep.name
      #os.symlink(dep, lnk)
      shutil.copy(dep, lnk)
  
  for mangled in data.tests:
    results = entry.get(mangled)
    # TODO: Actually do stuff
    run_individual_test(mangled, name, input, outpath, results)
  pass

# The default program entry point
def runner_main(args: ArgNamespace, extra_args: dict, /):
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
