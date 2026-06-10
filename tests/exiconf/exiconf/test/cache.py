import json, fnmatch
from pathlib import Path
from exiconf.constants import EXI_VERSION, TEST_OUT_DIR2
from exiconf.logging import errs, outs
from exiconf.version_tuple import version_tuple, VersionTuple

__all__ = ['ProcessCache', 'ProcessCacheResults']

CACHE_VERSION = '1.1'
CACHE_VERSION_INFO = version_tuple(CACHE_VERSION)
CACHE_MIN_VERSION = version_tuple('1.0')

"""
cache: {
  "version" : "...",
  "data" : {
    "at.at-01": {
      "__info__" : {
        "path": "at/at-01.xml",
        ...
      },
      "OyPcdip": {
        passed: ["i", "oi", "ii", ...],
        failed: ["xx", "ix", ...]
      },
    },
    ...
  }
}

__info__ {
  path: Path
  passed: bool
  ent?: list[Path]
  passed_with?: ...
}
"""

class ProcessCacheResults:
  __slots__ = ('_passed', '_failed',)
  _passed: set[str]
  _failed: set[str]

  def __init__(self, obj=None):
    # Check if we passed in an object
    if obj is not None:
      # Init passed entries
      if 'passed' in obj:
        self._passed = set(obj['passed'])
      else:
        self._passed = set()
      # Init failed entries
      if 'failed' in obj:
        self._failed = set(obj['failed'])
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
  
  # Clears everything
  def clear(self):
    self._passed.clear()
    self._failed.clear()
  
  def getdict(self):
    return {
      'passed': sorted(self._passed),
      'failed': sorted(self._failed),
    }
# end ProcessCacheResults

class ProcessCacheInfo:
  __slots__ = ('_path', '_passed', '_ent',)
  _path: Path | None
  _passed: bool
  _ent: list[Path]

  def __init__(self, obj=None):
    self._ent = None # type: ignore
    if obj is not None:
      # Get the real filepath
      if 'path' in obj:
        self._path = Path(obj['path'])
      else:
        self._path = None
      # Check if all entries already passed
      if 'passed' in obj:
        self._passed = bool(obj['passed'])
      else:
        self._passed = False
      # Check for .ent files
      if 'ent' in obj:
        ent = obj['ent']
        if len(ent) > 0:
          self._ent = [Path(f) for f in ent]
    else:
      self._path = None
      self._passed = False

  def add_ent(self, ent: Path):
    if ent is not None:
      self._ent.append(Path(ent))

  def getdict(self):
    out = {
      'path': None,
      'passed': self._passed,
    }
    if self._path is not None:
      out['path'] = self._path.as_posix()
    if self._ent and len(self._ent) > 0:
      out['ent'] = [f.as_posix() for f in self._ent]
    return out
# end ProcessCacheInfo

class ProcessCacheEntry:
  __slots__ = ('_results', '_info',)
  _results: dict[str, ProcessCacheResults]
  _info: ProcessCacheInfo | None

  def __init__(self, info=None, entries=None):
    self._results = {}
    self._info = info
    if entries is not None:
      for mangled, mangled_data in entries.items():
        self._results[mangled] = ProcessCacheResults(mangled_data)
  
  #@property
  #def results(self) -> dict[str, ProcessCacheResults]:
  #  return self._results
  
  def get(self, mangled: str) -> ProcessCacheResults:
    if mangled not in self._results:
      self._results[mangled] = ProcessCacheResults()
    return self._results[mangled]

  def add_ent(self, ent: Path):
    if self._info is not None:
      self._info.add_ent(ent)
  
  def _update_passed(self):
    pass

  def did_all_pass(self):
    pass
  
  def clear(self, to_clear=None):
    # Optimize 'NAME/*'
    if to_clear is None or to_clear == '*':
      self._results.clear()
      if self._info is not None:
        self._info._passed = False
      return
    
    # Find matching entries
    matched = fnmatch.filter(self._results.keys(), to_clear)
    #print(f' {to_clear}: {matched}')
    # Optimize not found
    if len(matched) == 0:
      return
    # Remove all matching
    for match in matched:
      del self._results[match]
    # Mark as dirty
    if self._info is not None:
      self._info._passed = False

  def getdict(self):
    if self._info is None:
      return self._results
    return {
      '__info__': self._info.getdict(),
      **self._results,
    }
# end ProcessCacheEntry

# Gets the cache path given an input
def _get_cache_path(cache_root) -> Path:
  if cache_root is None:
    return TEST_OUT_DIR2
  if type(cache_root) == str:
    if cache_root == '-':
      return TEST_OUT_DIR2
  return Path(cache_root)

# Gets exi_version, cache_version
def _process_cache_versions(cache_data) -> tuple[str, VersionTuple]:
  if '__version__' in cache_data:
    exi_version = cache_data['__version__']
    cache_version = version_tuple(cache_data['version'])
  else:
    exi_version = cache_data['version']
    cache_version = version_tuple('1.0')
  return exi_version, cache_version

# Converts the other format to the current one.
def _process_cache_data(cache_data) -> dict[str, ProcessCacheEntry]:
  out = {}
  for file_name, file_data in cache_data.items():
    info = None
    # Check if info exists
    if '__info__' in file_data:
      info = ProcessCacheInfo(file_data['__info__'])
      del file_data['__info__']
    out[file_name] = ProcessCacheEntry(info=info, entries=file_data)
  return out

def _parse_pattern(pattern: str) -> tuple[str, str | None]:
  split = pattern.split('/', 1)
  if len(split) == 1:
    return split[0], None
  else:
    return split[0], split[1]

class CacheJSONEncoder(json.JSONEncoder):
  def default(self, obj):
    if isinstance(obj, (ProcessCacheResults, ProcessCacheInfo, ProcessCacheEntry)):
      return obj.getdict()
    elif isinstance(obj, set):
      return sorted(obj)
    # Let the base class default method raise the TypeError
    return super().default(obj)
# end CacheJSONEncoder

class ProcessCache:
  __slots__ = ('_cache', '_root', '_cachefile',)
  _cache: dict[str, ProcessCacheEntry]
  #_files: set[str]
  _root: Path
  _cachefile: Path

  def __init__(self, cache_root=None):
    self._cache = {}
    #self._files = set([])
    # Init the root path
    self._root = _get_cache_path(cache_root)
    self._cachefile = self._root / '.cache.json'
  
  def __enter__(self):
    # Open cache!
    cachefile = self._cachefile
    if cachefile.exists():
      try:
        raw_text = cachefile.read_text(encoding='utf8')
        cache_data = json.loads(raw_text)
        # Get cache_version and exi_version
        exi_version, cache_version = _process_cache_versions(cache_data)
        # Check this is the correct version
        if cache_version >= CACHE_MIN_VERSION and exi_version == EXI_VERSION:
          self._cache = _process_cache_data(cache_data['data'])
          #if cache_data['files']:
          #  self._files.update(cache_data['files'])
      except json.JSONDecodeError as json_err:
        errs().error(json_err.msg)
        pass
      except FileNotFoundError:
        pass
    # ...
    if bool(self._cache):
      outs().info('Cache loaded from file.')
    else:
      outs().info('Cache is unset.')
    return self
  
  def __exit__(self, exc_type, exc_value, traceback):
    # TODO: Do stuff with exception values?
    self.save()
  
  # Gets the cache file given an input path
  @property
  def cachefile(self) -> Path:
    return self._cachefile

  def _clear_entry(self, pattern: str):
    # TODO: Handle 'at.at-01' & 'at.at-*/MANGLED' & 'at.at-01/*'
    name, entry = _parse_pattern(pattern)
    matched = fnmatch.filter(self._cache.keys(), name)
    #print(f'{name}: {matched}')

    # Remove entire entry
    if entry is None:
      for match in matched:
        if match in self._cache:
          del self._cache[match]
      return
    
    # Remove subentries
    for match in matched:
      self._cache[match].clear(entry)

  # Clears entry (or entries)
  def clear(self, to_clear=None):
    # Clear single entry
    if type(to_clear) == str:
      self._clear_entry(to_clear)
      return
    
    # Clear everything
    if to_clear is None or len(to_clear) == 0:
      outs().info('Clearing cache.')
      self._cache.clear()
      #self._files.clear()
      return

    # Handle multiple patterns
    for pattern in to_clear:
      if type(pattern) == str:
        self._clear_entry(pattern)
      else:
        errs().warn(f'Invalid pattern: {pattern}')

  # Prints cache data
  def dump(self):
    raw_text = json.dumps(self._cache,
      cls=CacheJSONEncoder, indent=2)
    outs().info(raw_text)

  # Saves cache to file
  def save(self):
    cachefile = self._cachefile
    raw_text = json.dumps({
      '__version__': EXI_VERSION,
      'version': CACHE_VERSION,
      #'files': self._files,
      'data': self._cache
    }, cls=CacheJSONEncoder, indent=2
    )
    cachefile.write_text(raw_text, encoding='utf8')
  
  # Loads top level entry from cache:
  def get(self, name: str, filename=None) -> ProcessCacheEntry:
    #assert filename in subfolders
    if name in self._cache:
      return self._cache[name]
    # Make new entry:
    if filename is not None:
      info = ProcessCacheInfo({
        'path': Path(filename).absolute(),
        'passed': False,
      });
      self._cache[name] = ProcessCacheEntry(info=info)
    else:
      self._cache[name] = ProcessCacheEntry()
    # Return newly created entry
    return self._cache[name]
# end ProcessCache
