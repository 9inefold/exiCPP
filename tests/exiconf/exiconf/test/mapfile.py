import json, fnmatch, re, itertools, functools
from collections import ChainMap, OrderedDict
from copy import deepcopy
from glob import glob
from pathlib import Path
from exiconf.constants import TEST_SRC_DIR
from exiconf.logging import errs, outs

from typing import TypeAlias, NewType, TypeGuard, cast
from collections.abc import Callable

__all__ = ['map_files', 'MappingData', 'MappingDataEntry']

PERMISSIVE = True

ALLOWED_ALIGNMENT = set('iypc')
ALLOWED_PRESERVE = set('cdilp')

# 0: str, 1: bool, 2: int | null, 3: preserve
KNOWN_ITEMS = {
  # TODO: Add a field for the parser exclude
  'alignment': 0,
  'strict': 1, 'selfcontained': 1,
  'blocksize': 2, 'maxlength': 2, 'partitioncapacity': 2,
  'preserve': 3,
}
KNOWN_DIRECTIVES = set([
  '#depends', '#import', '#exclude', '#include'
])

ORDERED_ITEMS = [
  'alignment',
  'strict',
  'selfcontained',
  'preserve',
  'blocksize',
  'maxlength',
  'partitioncapacity',
]
REVERSE_KNOWN_ITEMS_DEFAULTS = ['', False, None]
ORDERED_ITEMS_TYPES = [
  KNOWN_ITEMS[name] for name in ORDERED_ITEMS
]
REVERSE_ORDERED_ITEMS = {
  name: i for i, name in enumerate(ORDERED_ITEMS)
}

TOP_LEVEL = {
  REVERSE_ORDERED_ITEMS["alignment"]: { "i", "y", "p", "c" },
  #"strict": False,
  #"selfcontained": False,
  REVERSE_ORDERED_ITEMS["preserve"]: {
    "value": { "c", "d", "l", "i", "p" },
    "fixed": set(),
    "nullable": True,
    "combinations": True,
  },
  #"blocksize": None,
  #"maxlength": None,
  #"partitioncapacity": None
}

#LOG = outs().info

def _check_directive(name: str) -> bool:
  if name.startswith('#'):
    if name in KNOWN_DIRECTIVES:
      errs().warn(f"Directive '{name}' cannot be used here")
    else:
      errs().warn(f"Unknown directive '{name}'")
    return True
  return False

def _json_ordered(raw_data: str) -> OrderedDict:
  return json.loads(raw_data, object_pairs_hook=OrderedDict)

def _as_list(item) -> list[str]:
  """Converts str to [str], otherwise nothing"""
  if type(item) == str:
    return [item]
  return item

def _is_dict(obj) -> TypeGuard[OrderedDict[str, object]]:
  """Checks if obj is a dict/OrderedDict"""
  return isinstance(obj, OrderedDict)

#########################################################

ITEM_DIRECTIVES = { '#include', '#exclude' }
PRESERVE_DIRECTIVES = { '#include', '#exclude', '#fixed', 'nullable', 'combinations' }

def log_permissive(*args, **kwargs):
  if not PERMISSIVE:
    out = errs()._print_str(*args, **kwargs)
    raise ValueError(out)
  errs().error(*args, **kwargs)

def _make_type_check(chk_impl: Callable[[str, object], bool]):
  def list_or_val(name: str, mod) -> bool:
    if type(mod) == list:
      for el in mod:
        if not chk_impl(name, el):
          return False
      return True
    else:
      return chk_impl(name, mod)
  # dict, list, or value
  def checker(name: str, mod) -> bool:
    if _is_dict(mod):
      unknown_keys = []
      for key, value in mod.items():
        if key not in ITEM_DIRECTIVES:
          errs().warn(f"'{name}' unknown directive '{key}'")
          unknown_keys.append(key)
          continue
        if _is_dict(value):
          errs().warn(f"'{name}.{key}' value should not be a dict")
          unknown_keys.append(key)
          continue
        if not list_or_val(f'{name}.{key}', value):
          return False
      # Remove unknown keys
      for key in unknown_keys:
        del mod[key]
      return True
    else:
      return list_or_val(name, mod)
  # Generate new function
  return checker

def _type_check_str_i(name: str, mod) -> bool:
  if type(mod) != str:
    log_permissive(f"'{name}' expected str, got '{type(mod)}'")
    return False
  if name.startswith('alignment'):
    if mod in ALLOWED_ALIGNMENT:
      return True
    log_permissive(f"'{name}' contains invalid align, got '{mod}'")
  else:
    return True
  return False

def _type_check_bool_i(name: str, mod) -> bool:
  if type(mod) == bool:
    return True
  # TODO: Support truthy/falsy?
  #elif PERMISSIVE and isinstance(mod, (int, type(None))):
  #  errs().warn(f"'{name}' coercing '{type(mod)}' to bool")
  #  return True
  log_permissive(f"'{name}' expected bool, got '{type(mod)}'")
  return False

def _type_check_int_i(name: str, val) -> bool:
  if val is None:
    return True
  elif type(val) == int:
    if val > 0:
      return True
    log_permissive(f"'{name}' expected int >= 0, got '{val}'")
  else:
    # TODO: Allow values like "100"?
    log_permissive(f"'{name}' expected int/null, got '{type(val)}'")
  return False

def _type_check_preserve_i(name: str, mod) -> bool:
  if type(mod) != str:
    log_permissive(f"'{name}' expected str, got '{type(mod)}'")
    return False
  if all([c in ALLOWED_PRESERVE for c in mod]):
    return True
  log_permissive(f"'{name}' contains invalid preserve(s), got '{mod}'")
  return False

def _type_check_preserve_l(name: str, mod) -> bool:
  assert not _is_dict(mod)
  if type(mod) == list:
    for el in mod:
      if not _type_check_preserve_i(name, el):
        return False
    return True
  return _type_check_preserve_i(name, mod)

_type_check_str = _make_type_check(_type_check_str_i)
_type_check_bool = _make_type_check(_type_check_bool_i)
_type_check_int = _make_type_check(_type_check_int_i)

def _type_check_preserve(name: str, mod) -> bool:
  if not _is_dict(mod):
    return _type_check_preserve_l(name, mod)
  # It is a dict
  unknown_keys = []
  for key, value in mod.items():
    if key not in PRESERVE_DIRECTIVES:
      errs().warn(f"'{name}' unknown directive '{key}'")
      unknown_keys.append(key)
      continue
    # Check the type
    if key.startswith('#'):
      if _is_dict(value):
        if key != '#fixed':
          errs().error(f"'{name}' directive '{key}' cannot be a dict")
          unknown_keys.append(key)
          continue
        # TODO: 
        #PRESERVE_DIRECTIVES.difference()
        errs().warn('#fixed dict checking not implemented')
        continue
      if not _type_check_preserve_l(f'{name}.{key}', value):
        return False
    elif type(value) != bool:
      log_permissive(f"'{name}.{key}' expected bool, got '{type(value)}'")
      return False
  # Remove unknown keys
  for key in unknown_keys:
    del mod[key]
  return True

ITEM_TYPE_MAP = [
  _type_check_str,
  _type_check_bool,
  _type_check_int,
  _type_check_preserve,
]

def _type_check(name: str, mod) -> bool:
  ty = KNOWN_ITEMS[name]
  if ty < len(ITEM_TYPE_MAP):
    return ITEM_TYPE_MAP[ty](name, mod)
  else:
    log_permissive(f"unknown type number '{ty}'")
    return False

################################################################################

assert REVERSE_ORDERED_ITEMS["alignment"] == 0
assert REVERSE_ORDERED_ITEMS["preserve"] == 3

def _update_trans_dict_item(s: set[object], key: str, value: object):
  if type(value) == list:
    if key[1] == 'i':
      s.update(value)
    else:
      s -= set(value)
  else:
    if key[1] == 'i':
      s.add(value)
    elif value in s:
      s.remove(value)

# TODO: Make more efficient?
def _copy_trans_dict(new: dict[int, object], mod: OrderedDict[str, object], key_id: int):
  s: set[object]
  if key_id in new:
    s = cast(set, deepcopy(new[key_id]))
  else:
    typ = ORDERED_ITEMS_TYPES[key_id]
    s = { REVERSE_KNOWN_ITEMS_DEFAULTS[typ] }
  new[key_id] = s
  # Update the set
  for key, value in mod.items():
    _update_trans_dict_item(s, key, value)

def _copy_preserve(new: dict[int, object], mod: OrderedDict[str, object]):
  # 3 should be good
  preserve = cast(dict[str, set[str] | bool], new[3]).copy() # type: ignore
  s = cast(set[str], preserve['value']).copy()
  new[3] = preserve
  preserve['value'] = s  # type: ignore
  for key, value in mod.items():
    if key[0] == '#':
      if key[1] != 'f':
        # Must be simple
        _update_trans_dict_item(cast(set, s), key, value)
        continue
      # Must be fixed
      if _is_dict(value):
        f = cast(set, preserve['fixed']).copy()
        preserve['fixed'] = f
        for key, value in value.items():
          _update_trans_dict_item(f, key, value)
        continue
      # Replace the list
      if type(value) == list:
        preserve['fixed'] = set(value)
      else:
        preserve['fixed'] = { cast(str, value) }
      continue
    # Try other properties
    value = cast(bool, value)
    if key[0] == 'n': # nullable
      preserve['nullable'] = value
    else: # combinations
      preserve['combinations'] = value
    pass
  # TODO: Deduplicate value/fixed?
  pass

################################################################################

if not hasattr(fnmatch, '_compile_pattern'):
  @functools.lru_cache(maxsize=32768, typed=True)
  def _compile_pattern(pat) -> Callable[[str], re.Match | None]:
    if isinstance(pat, bytes):
      pat_str = str(pat, 'ISO-8859-1')
      res = fnmatch.translate(pat_str)
    else:
      res = fnmatch.translate(pat)
    return re.compile(res).match # type: ignore
else:
  _compile_pattern = getattr(fnmatch, '_compile_pattern')

def _get_none_key(val):
  return val if val is not None else -1

#class MapfileJSONEncoder(json.JSONEncoder):
#  def default(self, obj):
#    if isinstance(obj, set):
#      return sorted(obj, key=_get_none_key)
#    # Let the base class default method raise the TypeError
#    return super().default(obj)

NameID = NewType('NameID', int)
TransID = NewType('TransID', int)

# TODO: Use ChainMap?
class TransitionMap:
  """
  Handles the transitions between change records.

  Attributes
  ----------
  _files : dict[str, int]
    Maps files to an entry in the map table
  _maps : list[dict]
    The table of unique changes
  _transitions : list[list[int]]
    The list of transitions from each table entry
  """
  __slots__ = (
    '_ffiles', '_ids', '_ex_ids', '_files',
    '_maps', #'_transitions',
  )
  # id -> name
  _ffiles: list[str]
  # id -> trans_id
  _ids: list[TransID]
  _ex_ids: set[NameID]
  # name -> id
  _files: dict[str, NameID]
  # trans_id -> trans
  _maps: list[dict[int, object]]
  # trans_id -> [trans_id...]
  # TODO: Implement transitions
  _transitions: list[list[TransID]]

  def __init__(self, names: list[str], /):
    self._ffiles = names[:]
    self._ids = [TransID(0)] * len(names)
    self._ex_ids = set()
    self._files = { name: NameID(id) for id, name in enumerate(self._ffiles) }
    self._maps = [deepcopy(TOP_LEVEL)]
    #self._transitions = [[]]
  
  @property
  def maps(self) -> list[dict[int, object]]:
    return self._maps[:]
  
  def _is_removed_id(self, id: int) -> bool:
    return id in self._ex_ids
  
  def match(self, pattern: str, /) -> list[str]:
    out: list[str] = []
    match = _compile_pattern(pattern)
    for key in self._files.keys():
      if match(key):
        out.append(key)
    return out
  
  def match_ids(self, pattern: str, /) -> list[NameID]:
    out: list[NameID] = []
    match = _compile_pattern(pattern)
    for key, id in self._files.items():
      if match(key):
        out.append(id)
    return out
  
  def match_ids_category(self, pattern: str, /) -> dict[TransID, list[NameID]]:
    out: dict[TransID, list[NameID]] = {}
    match = _compile_pattern(pattern)
    for key, id in self._files.items():
      if match(key):
        tid = self._ids[id]
        if tid in out:
          out[tid].append(id)
        else:
          out[tid] = [id]
    return out

  def categorize_ids(self, ids: list[NameID], /) -> dict[TransID, list[NameID]]:
    out: dict[TransID, list[NameID]] = {}
    for id in ids:
      if self._is_removed_id(id):
        continue
      tid = self._ids[id]
      if tid in out:
        out[tid].append(id)
      else:
        out[tid] = [id]
    return out
  
  def categorized_ids(self) -> dict[TransID, list[NameID]]:
    out: dict[TransID, list[NameID]] = {}
    for id, tid in enumerate(self._ids):
      if self._is_removed_id(id):
        continue
      if tid in out:
        out[tid].append(NameID(id))
      else:
        out[tid] = [NameID(id)]
    return out

  def resolve_ids(self, ids: list[NameID], /) -> list[str]:
    return [self._ffiles[id] for id in ids]
  
  def _make_new_trans(self, new: dict[int, object]) -> TransID:
    self._maps.append(new)
    #self._transitions.append([])
    return TransID(len(self._maps) - 1)

  def update_ids(self, new: dict[int, object], ids: list[NameID], /):
    new_tid = self._make_new_trans(new)
    # Now update everything
    #old_tids: list[int] = []
    for id in ids:
      # Add the old id
      #old_tids.append(self._ids[id])
      self._ids[id] = new_tid
    # Create new transitions
    #for tid in set(old_tids):
    #  self._transitions[tid].append(new_tid)
  
  def get_trans(self, tid: TransID, /) -> dict[int, object]:
    assert tid < len(self._maps)
    return self._maps[tid]

class DependenciesMap:
  """
  Handles #depends for various files.

  Attributes
  ----------
  _deps : dict[str, set[Path]]
    The table mapping files to their dependencies
  _checked : dict[str, bool]
    The set of previously checked files
  """
  __slots__ = ('_root', '_deps', '_checked',)
  _root: Path
  _deps: dict[str, set[Path]]
  _checked: dict[str, bool]

  def __init__(self, root: Path):
    self._root = root
    self._deps = {}
    self._checked = {}
  
  def add(self, files: list[str], deps: list[str]):
    """For each file of files, add the dependencies"""
    dep_paths = self._filter(deps)
    if len(dep_paths) == 0:
      return
    ##LOG(f"#depends: {files} -> {dep_paths}")
    # Add deps to files
    for file in files:
      if file in self._deps:
        self._deps[file].update(dep_paths)
      else:
        self._deps[file] = set(dep_paths)
  
  def get(self, file: str) -> set[Path] | None:
    if file in self._deps:
      return self._deps[file]
    return None

  def _filter(self, deps: list[str]) -> list[Path]:
    """Filters out invalid dependencies"""
    out = []
    for dep in deps:
      dep_path = self._root / dep
      # Verify the file hasn't been checked
      if dep in self._checked and self._checked[dep]:
        # Add if valid
        out.append(dep_path)
        continue
      # Check if dep exists
      if not dep_path.exists():
        errs().warn(f"dependency '{dep}' does not exist")
        self._checked[dep] = False
        continue
      # Check if dep is file
      # TODO: Support folders?
      if not dep_path.is_file():
        errs().warn(f"dependency '{dep}' is not a file")
        self._checked[dep] = False
        continue
      # Add to lists
      errs().extra(f"dependency '{dep}' is valid!")
      out.append(dep_path)
      self._checked[dep] = True
    return out
  
class MappingMapper:
  """
  Handles the parsing of map.json files.

  Attributes
  ----------
  _root : Path
    Path to the base map.json file's folder
  
  _maps : TransitionMap
    The table of unique changes
  _excluded : set[str]
    The set of all entries ignored with #exclude
  
  _curr : OrderedDict
    The dict currently used for parsing
  _relative_to : Path
    The folder the map.json file is in
  _parse_stack : list[list[str]]
    The list of files queued to be parsed
  _inc_stack
    The stack of included folders
  """
  __slots__ = (
    '_root', '_curr_root',
    '_maps', '_dependencies', '_excluded',
    '_curr', '_curr_stack',
    '_relative_to',
    #'_parse_stack', '_inc_stack',
  )
  _root: Path
  _curr_root: Path
  _maps: TransitionMap
  _dependencies: DependenciesMap
  _excluded: set[str]

  _curr: OrderedDict[str, object]
  _curr_stack: list[OrderedDict[str, object]]
  _relative_to: str
  _parse_stack: list[list[str]]
  _inc_stack: list[str]

  def __init__(self, root: Path, names: list[str]):
    assert root is not None
    self._root = Path(root)
    self._curr_root = self._root
    self._maps = TransitionMap(names)
    self._dependencies = DependenciesMap(root)
    self._excluded = set()

    self._curr = None # type: ignore
    self._curr_stack = []
    self._relative_to = ''
    #self._parse_stack = [[]]
    #self._inc_stack = []
  
  @property
  def root(self) -> str:
    return self._root.as_posix()
  
  @property
  def maps(self):
    return self._maps.maps

  def at_root(self) -> bool:
    return self._relative_to == '.'
    
  def _relative(self, path: str) -> str:
    """Converts relative path-like strings to root-relative"""
    if self.at_root():
      return path
    return f'{self._relative_to}/{path}'
  
  @property
  def scope(self) -> int:
    if self._curr is not None:
      return len(self._curr_stack) + 1
    return 0
  
  def scope_begin(self, item: OrderedDict):
    if self._curr is not None:
      self._curr_stack.append(self._curr)
    self._curr = item
  
  def scope_end(self):
    if len(self._curr_stack) != 0:
      self._curr = self._curr_stack.pop()
    else:
      self._curr = None # type: ignore
  
  def _load(self, filepath: Path, name='map.json') -> bool:
    """Loads PATH/map.json into _curr"""
    # TODO: Allow alternate filenames?
    file = filepath / name
    # Simple checks
    if not file.exists():
      errs().error(f"'{file.as_posix()}' does not exist")
      return False
    if not file.is_file():
      errs().error(f"'{file.as_posix()}' is not a file")
      return False
    # Check the folder is relative to this one
    try:
      rel = filepath.relative_to(self.root)
      raw_data = file.read_text(encoding='utf8')
      # TODO: Handle parse stack
      self._curr_root = filepath
      self._relative_to = rel.as_posix()
      #LOG(f"Loaded '{file.as_posix()}'")
    except ValueError:
      errs().error(f"'{file.as_posix()}' is not relative to root")
      return False
    # Now load the file
    assert self.scope == 0
    self._curr = _json_ordered(raw_data)
    return True
  
  def _take(self, key: str):
    """
    Removes and returns a key from _curr, if it exists.
    Returns None otherwise.
    """
    if key in self._curr:
      out = self._curr[key]
      del self._curr[key]
      return _as_list(out)
    return None
  
  # Take "Non Null"
  def _take_nn(self, key: str):
    """
    Removes and returns a key from _curr, if it exists.
    Returns [] otherwise
    """
    if key in self._curr:
      out = self._curr[key]
      del self._curr[key]
      return _as_list(out)
    return []

  def _take_direct(self, key: str):
    """
    Removes and returns a key from _curr, if it exists.
    Does no list conversions
    """
    if key in self._curr:
      out = self._curr[key]
      del self._curr[key]
      return out
    return None

  def _ignore_toplevel(self):
    """Handles top-level #ignores"""
    # Get the exclusions as a list
    excludes = self._take_nn('#ignore')
    if len(excludes) == 0:
      return
    # Set up our exclusions
    maps = self._maps
    names = maps._files.keys()
    for pattern in excludes:
      pat = self._relative(pattern)
      matched = fnmatch.filter(names, pat)
      if len(matched) == 0:
        continue
      #LOG(f' #ignore: {pat} -> {matched}')
      self._excluded.update(matched)
      # TODO: Add maps.remove(...)?
      for match in matched:
        id = maps._files[match]
        maps._ex_ids.add(id)
        del maps._files[match]

  def _depends(self, files: list[str], deps: list[str]):
    if len(deps) == 0:
      return
    fdeps = [self._relative(f) for f in deps]
    self._dependencies.add(files, fdeps)

  # list of [(key_id, mod)...]
  def _get_known_keys(self) -> list[tuple[int, object]]:
    #mods = self._curr
    known_keys: list[tuple[int, object]] = []
    # Read changes
    for name, mod in self._curr.items():
      name = name.lower()
      if _check_directive(name):
        continue
      if name not in KNOWN_ITEMS:
        errs().warn(f"unknown item '{name}'")
        continue
      # Validate, will raise if not permissive
      if not _type_check(name, mod):
        continue
      # Good key
      curr_key = REVERSE_ORDERED_ITEMS[name]
      if isinstance(mod, (list, OrderedDict)):
        known_keys.append((curr_key, mod))
      else:
        known_keys.append((curr_key, [mod]))
    #known_keys.sort()
    return known_keys

  # Per-entry iteration
  def _do_entry(self, pattern: str):
    """Run the parser on "PATH": { ...changes... }"""
    assert self.scope == 2
    if len(self._curr) == 0:
      errs().warn(f"empty rule for '{pattern}'")
      return
    
    matched = self._maps.match_ids(pattern)
    deps = self._take('#depends')
    if len(matched) == 0:
      #LOG(f" '{pattern}' -> #0")
      if deps is not None:
        errs().warn(f"#depends provided but '{pattern}' matched no files")
      return
    
    # Handle #depends
    if deps is not None:
      named = self._maps.resolve_ids(matched)
      self._depends(named, deps)
    # TODO: Add #reset

    #LOG(f" '{pattern}' -> #{len(matched)}")
    known_keys = self._get_known_keys()
    # Reverse map in good order
    cat_ids = self._maps.categorize_ids(matched)
    # maps transition -> [id...]
    for tid, ids in cat_ids.items():
      new = self._maps.get_trans(tid).copy()
      # maps entry -> modifications
      for key_id, mod in known_keys:
        #LOG(f"    {ORDERED_ITEMS[key_id]} {key_id}")
        is_preserve = (ORDERED_ITEMS_TYPES[key_id] == 3)
        if type(mod) == list:
          if not is_preserve:
            new[key_id] = set(mod)
          else:
            p = cast(dict, new[3]).copy()
            new[3] = p
            p['value'] = set(mod) # type: ignore
          continue        
        #assert _is_dict(mod)
        mod = cast(OrderedDict[str, object], mod)
        if is_preserve:
          _copy_preserve(new, mod)
        else:
          _copy_trans_dict(new, mod, key_id)  
      # Now create a new entry
      self._maps.update_ids(new, ids)
      ##LOG("new:", json.dumps(new, cls=MapfileJSONEncoder, indent=1))
    pass

  # Top-level iteration
  def _do(self, filepath: Path) -> bool:
    """Run the parser on a single file"""
    if not self._load(filepath):
      # TODO: Add permissive mode
      return False
    self._ignore_toplevel()
    # Get imports
    imports = self._take('#import')
    if imports is not None:
      errs().warn('#import is unimplemented!')

    curr = self._curr
    for pattern, mods in curr.items():
      if _check_directive(pattern):
        continue
      if not _is_dict(mods):
        errs().error(f'expected dict, got: {type(mods)}')
        continue
      # Create temp scope
      self.scope_begin(mods)
      self._do_entry(self._relative(pattern))
      self.scope_end()
      #del self._curr[pattern]
    # Success!
    self.scope_end()
    return True

  def start(self):
    if not self._do(self._root):
      raise RuntimeError(f"Failed to load root '{self.root}'")
    # TODO: Handle imports?

ITEMS_PREFIXES = ['', 'S', 'C', 'P', 'B', 'M', 'V']

@functools.lru_cache
def _dedupe_string(s: str) -> str:
  return ''.join(sorted(set(s)))

def _generate_preserve(data) -> list[str]:
  out: set[str] = set()
  fixed = _dedupe_string(''.join(data['fixed']))
  if data['nullable']:
    out.add('')
  value: set[str] = data['value']
  if not data['combinations']:
    out.update([
      _dedupe_string('P' + v + fixed)
      for v in value
    ])
    return sorted(out)
  # Handle all combinations
  for n in range(1, len(value) + 1):
    it = itertools.combinations(value, n)
    out.update([
      _dedupe_string('P' + ''.join(c) + fixed)
      for c in it
    ])
  return sorted(out)

def _generate_option(key_id: int, data) -> list[str]:
  pfx = ITEMS_PREFIXES[key_id]
  match key_id:
    case 0:
      return sorted(data)
    case 1 | 2:
      out = []
      if False in data:
        out.append('')
      if True in data:
        out.append(pfx)
      return out
    case 3:
      return _generate_preserve(data)
    case 4 | 5 | 6:
      out = []
      items: list[int | None] = sorted(data, key=_get_none_key)
      for item in items:
        if item is None:
          out.append('')
        else:
          out.append(f'{pfx}{item}')
      return out
    case _:
      raise ValueError(f"invalid keyid '{key_id}'")

def _generate_options(m: dict[int, object]):
  out: list[list[str]] = []
  for entry_num in range(len(KNOWN_ITEMS)):
    if entry_num not in m:
      continue
    # Does exist, get the value
    data = m[entry_num]
    gen = _generate_option(entry_num, data)
    if len(gen) != 0:
      out.append(gen)
  return [''.join(t) for t in itertools.product(*out)]

class MappingDataEntry:
  __slots__ = ('_file', '_tests', '_deps',)
  _file: Path
  _tests: list[str]
  _deps: set[Path] | None

  def __init__(self, file: Path, tests: list[str], deps: set[Path] | None, /):
    self._file = file
    self._tests = tests
    self._deps = deps
  
  @property
  def file(self) -> Path:
    return self._file

  @property
  def tests(self) -> list[str]:
    return self._tests
  
  @property
  def dependencies(self) -> set[Path] | None:
    return self._deps

class MappingData:
  __slots__ = ('_root', '_data', '_deps',)
  # [(tests, files)...]
  _root: Path
  _data: list[tuple[list[str], list[str]]]
  _deps: DependenciesMap

  def __init__(self, mapper: MappingMapper):
    self._root = mapper._root
    self._deps = mapper._dependencies
    self._data = []
    maps = mapper.maps
    categorized = mapper._maps.categorized_ids()
    #LOG(f"Got {len(mapper._maps._files)} files")
    #LOG(f"Got {[len(v) for v in categorized.values()]} files")
    for tid, ids in categorized.items():
      tests = _generate_options(maps[tid])
      resolved = mapper._maps.resolve_ids(ids)
      self._data.append((tests, resolved))
      #LOG(f'{tid}: {tests}')
    #LOG()
  
  def keys(self):
    for _, files in self._data:
      for file in files:
        yield file.replace('/', '.')
  
  def values(self):
    for tests, files in self._data:
      for file in files:
        f = self._root / f'{file}.xml'
        deps = self._deps.get(file)
        yield MappingDataEntry(f, tests, deps)
  
  def items(self):
    for tests, files in self._data:
      for file in files:
        name = file.replace('/', '.')
        f = self._root / f'{file}.xml'
        deps = self._deps.get(file)
        yield name, MappingDataEntry(f, tests, deps)

def _map_files(root: Path, names: list[str]) -> MappingData:
  mapper = MappingMapper(root, names)
  mapper.start()
  #LOG()
  # Get all the generated data
  return MappingData(mapper)

# Loads file with glob pattern
def _load_glob_recursive(pattern: str, root) -> list[str]:
  all_files = glob(
    pattern,
    root_dir=root,
    recursive=True
  )
  # Convert to posix form
  out = [Path(f).as_posix() for f in all_files]
  out.sort()
  return out

# Public interface
def map_files(root=None) -> MappingData:
  if root is None:
    root = TEST_SRC_DIR
  root = Path(root)
  assert root.exists()
  # Check we have a base map.json
  if not (root / 'map.json').exists():
    raise FileNotFoundError(f"No map.json in '{root.as_posix()}'")
  
  # Get all the xml files
  xml_files = _load_glob_recursive('**/*.xml', root)
  xml_names = [f.removesuffix('.xml') for f in xml_files]
  # Map the filenames
  xml_mapping = {}
  for name, file in zip(xml_names, xml_files):
    xml_mapping[name] = file
  
  # Actually do the stuff
  # Runs in ~2.1ms
  return _map_files(root, xml_names)

"""
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
  xml_files = _load_glob_recursive('**/*.xml', root, sort=True)
  ent_files = set(_load_ext_recursive('ent', root))
  # Make a new list
  return [(f.replace('/', '.'), Path(f + '.xml'), f in ent_files) for f in xml_files]
"""


  
  
