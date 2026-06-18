import os, sys, shutil
import traceback, functools
from pathlib import Path
from jpype._core import JVMNotRunning
from exiconf.constants import *
from exiconf.cl_args import ArgNamespace
from exiconf.logging import outs, outs, Color
from .cache import *
from .mapfile import map_files, MappingDataEntry
from .counter import TestCounter

from exiconf.exicpp_coder import ExicppCoder, set_exicpp_verbosity
from exiconf.openexi_coder import OpenEXICoder
# from exiconf.exificient_coder import ExificientCoder

__all__ = ['runner_main', 'run_tests']

#OUT_DIR = TEST_OUT_DIR
OUT_DIR = TEST_OUT_DIR2

CODER_NAMES = [
  # Exicpp, OpenEXI, Exificient encode
  'i', 'o', #'x',
]
CODER_NAMES_KIND = {
  'i': 'exicpp',
  'o': 'openexi',
  'x': 'exificient',
}

class FatalException(RuntimeError):
  def __init__(self, id=None):
    if id is not None:
      super().__init__(f"Encountered fatal error while processing '{id}'")
    else:
      super().__init__(f"Encountered fatal error")

def is_fatal_exception(e: Exception) -> bool:
  return isinstance(e, (JVMNotRunning, FatalException))

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
  return cls(mangled, outs())

def run_individual_test(
    mangled: str, name: str, input: Path, outpath: Path,
    results: ProcessCacheResults, counter: TestCounter, /):
  # ...
  invalidated = set()
  encoded = []
  for typ in CODER_NAMES:
    k = CODER_NAMES_KIND[typ]
    id = f'{name}/{mangled}/{typ}'
    # Check file cache
    outfile = outpath / f'{mangled}.{typ}.exi'
    if results.did_pass(typ) and outfile.exists():
      outs().info(f"{id} skipped [{k}]", color=Color.BRIGHT_GREEN)
      # Skip work if we can
      encoded.append(typ)
      counter.add_skipped()
      continue
    
    # Actually run the coder
    coder = get_coder(typ, mangled)
    try:
      did_pass = coder.encode_file(input, outfile)
    except Exception as ex:
      if is_fatal_exception(ex):
        results.failed(typ)
        results.invalidate_all(invalidated)
        raise FatalException(id)
      did_pass = False
      outs().error(traceback.format_exc())
    
    if did_pass:
      results.passed(typ)
      encoded.append(typ)
      counter.add_passed()
      outs().info(f"{id} encode PASSED [{k}]", color=Color.BRIGHT_GREEN)
    else:
      results.failed(typ)
      counter.add_failed()
      outs().always(f"{id} encode FAILED [{k}]\n")

  decoded = []
  for enc in encoded:
    infile = outpath / f'{mangled}.{enc}.exi'
    for typ in CODER_NAMES:
      k = CODER_NAMES_KIND[typ]
      full_typ = enc + typ
      id = f'{name}/{mangled}/{full_typ}'

      # Check file cache
      outfile = outpath / f'{mangled}.{full_typ}.xml'
      if results.did_pass(full_typ) and outfile.exists():
        outs().info(f"{id} skipped [{k}]", color=Color.BRIGHT_GREEN)
        # Skip work if we can
        decoded.append(full_typ)
        counter.add_skipped()
        continue
      
      # Actually run the coder
      coder = get_coder(typ, mangled)
      try:
        did_pass = coder.decode_file(infile, outfile)
      except Exception as ex:
        if is_fatal_exception(ex):
          results.failed(typ)
          results.invalidate(enc)
          results.invalidate_all(invalidated)
          raise FatalException(id)
        did_pass = False
        outs().error(traceback.format_exc())
    
      if did_pass:
        results.passed(full_typ)
        decoded.append(full_typ)
        counter.add_passed()
        outs().info(f"{id} decode PASSED [{k}]", color=Color.BRIGHT_GREEN)
      else:
        invalidated.add(enc)
        results.failed(full_typ)
        counter.add_failed()
        outs().always(f"{id} decode FAILED [{k}]\n")
  
  # TODO: Actually do stuff
    
  # This means one of the dependent tests failed, so we should remove the item
  # from the passed list to ensure it gets regenerated.
  # Accidentally didn't do this originally, and spent 2 days trying to debug
  # an error I had already fixed in the first 30 minutes :(
  # FIXME: Make cache entries a "trie" to encode this directly?
  results.invalidate_all(invalidated)

def run_tests(name: str, data: MappingDataEntry, 
              entry: ProcessCacheEntry, counter: TestCounter, /):
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
    run_individual_test(
      mangled, name, input, outpath,
      results, counter)
  pass

def handle_clear(cache: ProcessCache, clear: list[str]):
  if len(clear) == 0:
    cache.clear()
  else:
    for to_clear in clear:
      cache.clear(to_clear)

def print_results(results: TestCounter):
  passed = results.total_passed
  total = results.total
  percent, color = results.percent_and_color()
  outs().always(
    '\nTEST RESULTS:',
    f'{passed}/{total} tests passed',
    f'{percent:.1f}% success',
    sep='\n', color=color)

# The default program entry point
def runner_main(args: ArgNamespace, extra_args: dict, /):
  file_map = map_files(root=args.root)
  with ProcessCache(args.cachefile) as cache:
    if args.clear is not None:
      handle_clear(cache, args.clear)
    set_exicpp_verbosity(args.x_diag_level)
    # Run the actual tests
    counter = TestCounter()
    for name, data in file_map.items():
      entry = cache.get(name, data.file)
      #if entry.did_all_pass():
      #  continue
      run_tests(name, data, entry, counter)
    # Print pass/fail info
    if args.print_results:
      print_results(counter)
