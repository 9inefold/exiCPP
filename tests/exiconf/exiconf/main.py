import json, os, sys, subprocess, traceback
from glob import glob
from pathlib import Path
#from subprocess import run as run_proc
from exiconf.constants import *
from exiconf.constants import EXI_VERSION
from exiconf.cl_args import parse_args
from exiconf.logging import outs

__all__ = ['main']

#OUT_DIR = TEST_OUT_DIR
OUT_DIR = EXI_BASE_DIR / 'tests/o2'
if not OUT_DIR.exists():
  OUT_DIR.mkdir(parents=True)

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

"""
NEW:
cache: {
  "version" : "...",
  "data" : {
    "at/at-01": {
      "OyPcdip" : {
        passed: ["i", "oi", "ii", ...],
        failed: ["xx", "ix", ...]
      },
    },
    ...
  }
}
"""

class ProcessCacheEntry:
  __slots__ = ('_name', '_passed', '_failed',)
  _passed: set[str]
  _failed: set[str]

  def __init__(self, obj=None):
    # Check if we passed in an object
    if obj is not None:
      # Init passed entries
      if 'passed' in obj and obj.passed is not None:
        self._passed = set(obj.passed)
      else:
        self._passed = set()
      # Init failed entries
      if 'failed' in obj and obj.failed is not None:
        self._failed = set(obj.failed)
      else:
        self._failed = set()
    # Otherwise, default init
    else:
      self._passed = set()
      self._failed = set()
    pass
  
  # Checks if work has already been done
  def did_work(self, name: str) -> bool:
    assert name is not None
    if name in self._passed:
      return True
    # Failed work should be repeated
    return False
  
  # Adds a new passed entry
  def passed(self, name: str):
    if name is not None:
      self._passed.add(name)
      if name in self._failed:
        self._failed.remove(name)
  
  # Adds a new failed entry
  def failed(self, name: str):
    if name is not None:
      self._failed.add(name)
      if name in self._passed:
        self._passed.remove(name)
  
  def getdict(self):
    return {
      'passed': sorted(self._passed),
      'failed': sorted(self._failed),
    }
# end ProcessCacheEntry

class CacheJSONEncoder(json.JSONEncoder):
  def default(self, obj):
    if isinstance(obj, ProcessCacheEntry):
      return obj.getdict()
    elif isinstance(obj, set):
      return sorted(obj)
    # Let the base class default method raise the TypeError
    return super().default(obj)
# end CacheJSONEncoder

class ProcessCache:
  __slots__ = ('_cache', '_files', '_cachefile',)
  _cache: dict[str, dict[str, ProcessCacheEntry]]
  _files: set[str]
  _cachefile: Path

  # Gets the cache path given an input
  def _get_cache_path(self, cache_path=None) -> Path:
    if cache_path is None:
      return OUT_DIR
    if type(cache_path) == str:
      if cache_path == '-':
        return OUT_DIR
    return Path(cache_path)
  
  # Gets the cache file given an input path
  def _get_cache_file(self, cache_path=None) -> Path:
    return self._get_cache_path(cache_path) / '.cache.json'

  def _process_cache_data(self, cache_data):
    for file_data in cache_data.values():
      for mangled, mangled_data in file_data.entries():
        cache_data[mangled] = ProcessCacheEntry(mangled_data)
    return cache_data

  def __init__(self, cache_path=None):
    self._cache = {}
    self._files = set([])
    self._cachefile = self._get_cache_file(cache_path)
  
  def __enter__(self):
    # Open cache!
    cachefile = self._cachefile
    if cachefile.exists():
      try:
        raw_text = cachefile.read_text(encoding='utf8')
        cache_data = json.loads(raw_text)
        if cache_data['version'] == EXI_VERSION:
          self._cache = self._process_cache_data(cache_data['data'])
          if cache_data['files']:
            self._files.update(cache_data['files'])
      except json.JSONDecodeError as json_err:
        qprint(json_err.msg)
        pass
      except FileNotFoundError:
        pass
    # ...
    if bool(self._cache):
      outs().info('Cache loaded from file.')
    else:
      outs().info('Cache is unset.')
  
  def __exit__(self, exc_type, exc_value, traceback):
    # TODO: Do stuff with exception values?
    self.save()
  
  # Clears entry (or entries)
  def clear(self, to_clear):
    if type(to_clear) == str:
      to_clear = [to_clear]
    # Clear everything
    if len(to_clear) == 0:
      outs().info('Clearing cache.')
      mappings: dict
      for mode, mappings in self._cache.items():
        vals: set
        for k, vals in mappings.items():
          if k != '__entry__':
            vals.clear()
      pass
    else:
      outs().info(f'Clearing entries {to_clear}.')
      mappings: dict
      for mode, mappings in self._cache.items():
        vals: set
        for k, vals in mappings.items():
          if k != '__entry__':
            vals.difference_update(to_clear)
      pass

  # Prints cache data
  def dump(self):
    raw_text = json.dumps(self._cache,
      cls=CacheJSONEncoder, indent=2)
    outs().info(raw_text)

  # Saves cache to file
  def save(self):
    cachefile = self._cachefile
    raw_text = json.dumps({
      'version': EXI_VERSION,
      'files': self._files,
      'data': self._cache
    }, cls=CacheJSONEncoder, indent=0)
    cachefile.write_text(raw_text, encoding='utf8')
  
  # Loads top level entry from cache:
  def load_entry(self, filename: str) -> dict[str, [str]]:
    #assert filename in subfolders
    if filename in self._cache:
      return self._cache[filename]
    # Make new entry:
    self._cache[name] = {}
    return self._cache[name]
# end ProcessCache

def _load_glob_recursive(pattern: str) -> list[str]:
  all_files = glob(
    pattern,
    root_dir=TEST_SRC_DIR,
    recursive=True
  )
  # Convert to posix form
  return sorted([Path(f).as_posix() for f in all_files])

# The default program entry point
def main():
  #parser = get_arg_parser()
  #print(parser.format_help(), flush=True)
  args = parse_args()

  xml_files = _load_glob_recursive('**/*.xml')
  ent_files = _load_glob_recursive('**/*.ent')
  print(xml_files)
  #print(ent_files)

  with ProcessCache(args.cachefile) as cache:
    # ...
    pass
