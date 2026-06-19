import os, sys, shutil
import traceback, functools
from pathlib import Path
from jpype._core import JVMNotRunning
from exiconf.constants import *
from exiconf.cl_args import ArgNamespace
from exiconf.logging import errs, outs, Color, LogLevel
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

OUTPUT = outs()

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
      return ExicppCoder(mangled, OUTPUT)
    case 'o':
      return OpenEXICoder(mangled, OUTPUT)
    case 'x':
      raise NotImplementedError('exificient coder is not implemented yet!')
    case _:
      raise ValueError(f"expected {CODER_NAMES}, got '{typ}'")

def run_individual_test(
    mangled: str, name: str, input: Path, outpath: Path,
    results: ProcessCacheResults, counter: TestCounter, /,
    print_passed: bool, print_skipped: bool):
  # ...
  os = OUTPUT
  invalidated = set()
  encoded = []
  for typ in CODER_NAMES:
    k = CODER_NAMES_KIND[typ]
    id = f'{name}/{mangled}/{typ}'
    # Check file cache
    outfile = outpath / f'{mangled}.{typ}.exi'
    if results.did_pass(typ) and outfile.exists():
      # Skip work if we can
      encoded.append(typ)
      counter.add_skipped()
      if print_skipped:
        os.info(f"{id} skipped [{k}]", color=Color.BRIGHT_GREEN)
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
      os.error(traceback.format_exc())
    
    if did_pass:
      results.passed(typ)
      encoded.append(typ)
      counter.add_passed()
      if print_passed:
        os.info(f"{id} encode PASSED [{k}]", color=Color.BRIGHT_GREEN)
    else:
      results.failed(typ)
      counter.add_failed()
      os.always(f"{id} encode FAILED [{k}]\n", color=Color.RED)

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
        # Skip work if we can
        decoded.append(full_typ)
        counter.add_skipped()
        if print_skipped:
          os.info(f"{id} skipped [{k}]", color=Color.BRIGHT_GREEN)
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
        os.error(traceback.format_exc())
    
      if did_pass:
        results.passed(full_typ)
        decoded.append(full_typ)
        counter.add_passed()
        if print_passed:
          os.info(f"{id} decode PASSED [{k}]", color=Color.BRIGHT_GREEN)
      else:
        invalidated.add(enc)
        results.failed(full_typ)
        counter.add_failed()
        os.always(f"{id} decode FAILED [{k}]\n", color=Color.RED)
  
  # TODO: Actually do stuff
    
  # This means one of the dependent tests failed, so we should remove the item
  # from the passed list to ensure it gets regenerated.
  # Accidentally didn't do this originally, and spent 2 days trying to debug
  # an error I had already fixed in the first 30 minutes :(
  # FIXME: Make cache entries a "trie" to encode this directly?
  results.invalidate_all(invalidated)

def run_tests(name: str, data: MappingDataEntry, 
              entry: ProcessCacheEntry, counter: TestCounter, /,
              print_passed, print_skipped):
  # TODO: Change working directory to outpath
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
      results, counter, print_passed, print_skipped)
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
  OUTPUT.always(
    '\nTEST RESULTS:',
    f'{passed}/{total} tests passed',
    f'{percent:.1f}% success',
    sep='\n', color=color)

# The default program entry point
def runner_main(args: ArgNamespace, extra_args: dict, /):
  # Set up the mapfiles
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
      run_tests(name, data, entry, counter,
                args.print_passed, args.print_skipped)
    # Print pass/fail info
    if args.print_results:
      print_results(counter)
