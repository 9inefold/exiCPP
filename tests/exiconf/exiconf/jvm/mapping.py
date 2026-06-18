import os, sys
from pathlib import Path
from glob import glob
from typing import Text
from exiconf.constants import EXI_BASE_DIR
from exiconf.logging import errs
from .setup import get_jvm_path
from typing import TYPE_CHECKING

__all__ = [
  'lookup_line',
  'lookup_line_ext'
]

JAVA_SEARCH_DIRS = [
  EXI_BASE_DIR/'tests/jarvis'
]

if TYPE_CHECKING:
  from typing import Optional, TypeAlias, TypeGuard, Union
  NormalFileMapping: TypeAlias = Union[
    'JavaRealFileMapping',
    'JavaMultiFileMapping',
  ]
  AnyFileMapping: TypeAlias = Union[
    'JavaRealFileMapping',
    'JavaMultiFileMapping',
    'JavaZipFileMapping'
  ]
  JavaFileLineDict: TypeAlias = dict[str, NormalFileMapping]

class JavaFileMapping:
  __slots__ = ('is_multi',)
  is_multi: bool

  def __init__(self, is_multi=False):
    self.is_multi = is_multi
  def get_line(self, lineno: int) -> str:
    return ''
  def get_line_ext(self, clazzname: str, lineno: int) -> str:
    return self.get_line(lineno)

# Maps lines in a JException to java files
class JavaRealFileMapping(JavaFileMapping):
  __slots__ = ('_package', '_filename', '_filedata',)
  _package: str
  _filename: Path
  _filedata: Optional[list[str]]

  def __init__(self, package: str, filename: Path):
    super().__init__(is_multi=False)
    self._package = str(package)
    self._filename = Path(filename)
    self._filedata = None

  @property
  def package(self) -> str:
    return self._package

  @property
  def filename(self) -> str:
    return self._filename.as_posix()

  @property
  def cache(self) -> list[str]:
    cache = self._filedata
    if cache is not None:
      return cache
    # Load the file data
    try:
      blob = self._filename.read_text(encoding='utf8')
      lines = blob.splitlines(keepends=True)
      if lines and not lines[-1].endswith('\n'):
        lines[-1] += '\n'
      self._filedata = lines
      return lines
    except (OSError, UnicodeDecodeError):
      # print('failure!')
      self._filedata = []
      return []

  def get_line(self, lineno: int) -> str:
    if lineno is None or lineno < 0:
      return ''
    cache = self.cache
    if lineno > 0:
      lineno -= 1
    if len(cache) <= lineno:
      return ''
    line = cache[lineno].strip()
    return line

# Maps multiple `JavaRealFileMapping`s to one filename
class JavaMultiFileMapping(JavaFileMapping):
  __slots__ = ('_mappings', '_mappingdict',)
  _mappings: list[JavaRealFileMapping]
  _mappingdict: dict[str, JavaRealFileMapping]

  def __init__(self, mappings: list[JavaRealFileMapping] = []):
    super().__init__(is_multi=True)
    assert isinstance(mappings, list)
    self._mappings = mappings[:]
    # Set up dict
    md = {}
    for mapping in self._mappings:
      pkg = mapping.package
      if pkg in self._mappingdict:
        errs().warn(f"Conflicting package '{pkg}' in JavaMultiFileMapping?")
        continue
      md[pkg] = mapping
    self._mappingdict = md
  
  def add(self, mapping: JavaRealFileMapping):
    if mapping is None:
      return
    if not isinstance(mapping, JavaRealFileMapping):
      errs().warn(f'{type(mapping)} passed to JavaMultiFileMapping?')
      return
    pkg = mapping.package
    if pkg in self._mappingdict:
      errs().warn(f"Conflicting package '{pkg}' in JavaMultiFileMapping?")
      return
    self._mappings.append(mapping)
    self._mappingdict[pkg] = mapping

  def get_line(self, lineno: int) -> str:
    if len(self._mappings) == 0:
      return ''
    # Always use the first if there is no way to disambiguate
    return self._mappings[0].get_line(lineno)
  
  def get_line_ext(self, clazzname: str, lineno: int) -> str:
    modulename, clazzname = clazzname.rsplit('.', 1)
    ppath = modulename.replace('.', '/')
    md = self._mappingdict
    if ppath in md:
      return md[ppath].get_line(lineno)
    return ''

# Checks if this is a multi-mapping
def _is_multimapping(obj: NormalFileMapping) -> TypeGuard[JavaMultiFileMapping]:
  return obj.is_multi

# Wraps behaviour for lines in a zipped file.
class JavaZipFileMapping(JavaFileMapping):
  __slots__ = ('_filedata',)
  _filedata: list[str]
  
  def __init__(self, filedata: list[str]):
    super().__init__(is_multi=False)
    assert filedata is not None
    self._filedata = filedata
  
  @property
  def cache(self) -> list[str]:
    return self._filedata

  def get_line(self, lineno: int) -> str:
    cache = self.cache
    if len(cache) == 0:
      return ''
    if lineno is None or lineno < 0:
      return ''
    if lineno > 0:
      lineno -= 1
    if len(cache) <= lineno:
      return ''
    line = cache[lineno].strip()
    return line

# Handles getting lines from files in a zipped source.
class JavaSrcZip:
  __slots__ = ('_zip', '_names', '_cache',)
  #_zip: zipfile.ZipFile
  _names: list[Text]
  _cache: dict[str, Optional[JavaZipFileMapping]]
  
  def __init__(self, _zip):
    self._zip = _zip
    if self._zip is not None:
      self._names = _zip.namelist()
    else:
      self._names = []
    self._cache = {}
  
  def _lookup(self, matches) -> Optional[bytes]:
    zf = self._zip
    for match in matches:
      try:
        return zf.read(match)
      except KeyError:
        pass
    return None

  def lookup(self, modulename: str, filename: str) -> Optional[JavaZipFileMapping]:
    if self._zip is None:
      return None
    if modulename in self._cache:
      return self._cache[modulename]
    # Get the relative path
    modulepath = modulename.replace('.', '/')
    modulepath += f'/{filename}'
    matches = [x for x in self._names if x.endswith(modulepath)]
    # Read from the zip
    data = self._lookup(matches)
    if data is None:
      return None
    # Parse lines from the zip
    try:
      blob = data.decode('utf8')
      lines = blob.splitlines(keepends=True)
      if lines and not lines[-1].endswith('\n'):
        lines[-1] += '\n'
      m = JavaZipFileMapping(lines)
      self._cache[modulename] = m
      return m
    except (OSError, UnicodeDecodeError):
      self._cache[modulename] = None
    return None

try:
  from zipfile import ZipFile # type: ignore
  _java_src_zip_tried_load = False
except (ModuleNotFoundError, ImportError):
  _java_src_zip_tried_load = True

_java_src_zip: JavaSrcZip = None # type: ignore
_java_file_line_dict: JavaFileLineDict = None # type: ignore

# Finds the JDK 'src.zip' file containing the sources.
def _find_java_src() -> Optional[Path]:
  jvm_path = get_jvm_path()
  if jvm_path is None:
    errs().error('JVM was not started!')
    return None
  java_home = Path(jvm_path).parent
  while java_home.name != 'bin':
    if len(java_home.parts) <= 1:
      return None
    java_home = java_home.parent
  java_home = java_home.parent
  # Find the file
  f = java_home/'src.zip'
  if f.exists() and f.is_file():
    return f
  f = java_home/'lib/src.zip'
  if f.exists() and f.is_file():
    return f
  # Shrug
  return None

# Finds 'src.zip', if not already loaded.
def _load_java_src() -> Optional[JavaSrcZip]:
  global _java_src_zip
  global _java_src_zip_tried_load
  # See if the file is cached
  if _java_src_zip_tried_load:
    return _java_src_zip
  # Try and load the file
  _java_src_zip_tried_load = True
  try:
    src_zip = _find_java_src()
    if src_zip is None:
      errs().info(f"unable to find 'src.zip'")
      _java_src_zip = JavaSrcZip(None)
      return None
    zf = ZipFile(str(src_zip), 'r')
    _java_src_zip = JavaSrcZip(zf)
    #print(zf.namelist())
    return _java_src_zip
  except Exception as e:
    errs().info(f"unable to load 'src.zip': {e}")
    return None

def _search_dir_for_java(search_dir: Union[str, Path], out: JavaFileLineDict):
  all_java_files = glob(
    '**/*.java',
    root_dir=str(search_dir),
    recursive=True
  )
  for java_file in all_java_files:
    # Path relative to tests/jarvis
    rpath = str(java_file)
    ppath = Path(rpath).parent.as_posix()
    # Absolute path
    abs_path = Path(search_dir)/rpath
    jfile = abs_path.name
    # Create a basic mapping
    mapping = JavaRealFileMapping(ppath, abs_path)
    # Check if this is a new file
    if jfile not in out:
      out[jfile] = mapping
      continue
    # Get the existing mapping
    other_mapping = out[jfile]
    if not _is_multimapping(other_mapping):
      out[jfile] = JavaMultiFileMapping([
        other_mapping, mapping # type: ignore
      ])
    else:
      # JavaMultiFileMapping
      other_mapping.add(mapping)

# Loads the file dict
def _load_file_dict() -> JavaFileLineDict:
  global _java_file_line_dict
  if _java_file_line_dict is not None:
    return _java_file_line_dict

  file_line_dict: JavaFileLineDict = {}
  for dir in JAVA_SEARCH_DIRS:
    # Add all the files found in a given folder
    _search_dir_for_java(dir, file_line_dict)
  
  _java_file_line_dict = file_line_dict
  return _java_file_line_dict

# Checks if class is any of the core java modules
def is_class_in_std_java_module(clazzname: str) -> bool:
  return clazzname.startswith((
    "java.", "javax.", "jdk.", "sun.", "com.sun."))

# Finds a `.java` file with class and filename, or `None` when not found
def lookup_ext(clazzname, filename: str) -> Optional[AnyFileMapping]:
  if filename is None or len(filename) == 0:
    return None
  d = _load_file_dict()
  if filename in d:
    return d[filename]
  if clazzname is None:
    return None
  
  clazzname = str(clazzname)
  modulename = clazzname.rsplit('.', 1)[0]
  # Check if this is a standard module
  if is_class_in_std_java_module(clazzname):
    src_zip = _load_java_src()
    if src_zip is None:
      return None
    return src_zip.lookup(modulename, filename)
  
  # Try finding module for some other source (unsupported)
  return None

# Finds a `.java` file with `filename`, or `None` when not found
def lookup(filename: str) -> Optional[AnyFileMapping]:
  return lookup_ext(None, filename)

# Looks up text at `lineno` from filename
def lookup_line(filename: str, lineno: int) -> str:
  m = lookup_ext(None, filename)
  if m is None:
    return ''
  return m.get_line(lineno)

# Looks up text at `lineno` with class and filename
def lookup_line_ext(clazz, filename: str, lineno: int) -> str:
  if clazz is None:
    return lookup_line(filename, lineno)
  clazzname = str(clazz)
  m = lookup_ext(clazzname, filename)
  if m is None:
    if _java_src_zip is not None:
      return '???'
    return '###'
  return m.get_line_ext(clazzname, lineno)
