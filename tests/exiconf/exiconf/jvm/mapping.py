import os, sys
from pathlib import Path
from glob import glob
from typing import Text
from exiconf.main import EXI_BASE_DIR
from exiconf.logging import errs
from exiconf.jvm.setup import get_jvm_path

__all__ = [
  'lookup_line',
  'lookup_line_ext'
]

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
  _filedata: list[str] | None

  def __init__(self, package: str, filename: Path):
    super().__init__()
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
    if cache is None or len(cache) <= lineno:
      return ''
    line = cache[lineno].strip()
    return line

# Maps multiple `JavaRealFileMapping`s to one filename
class JavaMultiFileMapping(JavaFileMapping):
  __slots__ = ('_mappings', '_mappingdict',)
  _mappings: list[JavaRealFileMapping]
  _mappingdict: dict[str, JavaRealFileMapping]

  def __init__(self, mappings: list[JavaRealFileMapping] | None = None):
    super().__init__(is_multi=True)
    if mappings is None:
      self._mappings = []
    else:
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

# Wraps behaviour for lines in a zipped file.
class JavaZipFileMapping(JavaFileMapping):
  __slots__ = ('_filedata',)
  _filedata: list[str]
  
  def __init__(self, filedata: list[str]):
    super().__init__()
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
  _cache: dict[str, JavaZipFileMapping | None]
  
  def __init__(self, _zip):
    self._zip = _zip
    if self._zip is not None:
      self._names = _zip.namelist()
    else:
      self._names = []
    self._cache = {}
  
  def __lookup(self, matches) -> bytes | None:
    zf = self._zip
    for match in matches:
      try:
        return zf.read(match)
      except KeyError:
        pass
    return None

  def lookup(self, modulename: str, filename: str) -> JavaZipFileMapping | None:
    if self._zip is None:
      return None
    if modulename in self._cache:
      return self._cache[modulename]
    # Get the relative path
    modulepath = modulename.replace('.', '/')
    modulepath += f'/{filename}'
    matches = [x for x in self._names if x.endswith(modulepath)]
    # Read from the zip
    data = self.__lookup(matches)
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

_java_file_line_dict: dict[str, JavaFileMapping]
_java_file_line_dict = None # type: ignore

_java_src_zip: JavaSrcZip
_java_src_zip = None # type: ignore
_java_src_zip_tried_load = False

# Finds the JDK 'src.zip' file containing the sources.
def _find_java_src() -> Path | None:
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
def _load_java_src() -> JavaSrcZip:
  global _java_src_zip
  global _java_src_zip_tried_load
  # See if the file is cached
  if _java_src_zip is not None:
    return _java_src_zip
  elif _java_src_zip_tried_load:
    return None
  # Try and load the file
  _java_src_zip_tried_load = True
  try:
    import zipfile
    src_zip = _find_java_src()
    if src_zip is None:
      errs().info(f"unable to find 'src.zip'")
      _java_src_zip = JavaSrcZip(None)
      return None
    zf = zipfile.ZipFile(str(src_zip), 'r')
    _java_src_zip = JavaSrcZip(zf)
    #print(zf.namelist())
    return _java_src_zip
  except Exception as e:
    errs().info(f"unable to load 'src.zip': {e}")
    return None

# Loads the file dict
def _load_file_dict() -> dict[str, JavaFileMapping]:
  global _java_file_line_dict
  if _java_file_line_dict is not None:
    return _java_file_line_dict

  SEARCH_DIR = EXI_BASE_DIR/'tests/jarvis'
  all_java_files = glob(
    '**/*.java',
    root_dir=SEARCH_DIR,
    recursive=True
  )

  file_line_dict: dict[str, JavaFileMapping]
  file_line_dict = {}
  for java_file in all_java_files:
    rpath = str(java_file)
    ppath = Path(rpath).parent.as_posix()
    fpath = SEARCH_DIR/rpath
    jfile = fpath.name
    mapping = JavaRealFileMapping(ppath, fpath)
    if jfile in file_line_dict:
      other_mapping = file_line_dict[jfile]
      if not other_mapping.is_multi:
        file_line_dict[jfile] = JavaMultiFileMapping([
          other_mapping, mapping
        ])
        continue
      # JavaMultiFileMapping
      other_mapping.add(mapping)
    else:
      file_line_dict[jfile] = mapping
  
  _java_file_line_dict = file_line_dict
  return _java_file_line_dict

# Checks if class is any of the core java modules
def is_class_in_std_java_module(clazzname) -> bool:
  if clazzname is None:
    return False
  return clazzname.startswith((
    "java.", "javax.", "jdk.", "sun.", "com.sun."))

# Finds a `.java` file with class and filename, or `None` when not found
def lookup_ext(clazzname, filename: str) -> JavaFileMapping | None:
  if filename is None or len(filename) == 0:
    return None
  d = _load_file_dict()
  if filename in d:
    return d[filename]
  if clazzname is None:
    return None
  
  clazzname = str(clazzname)
  modulename, _clazzname = clazzname.rsplit('.', 1)
  # Check if this is a standard module
  if is_class_in_std_java_module(clazzname):
    src_zip = _load_java_src()
    if src_zip is None:
      return None
    return src_zip.lookup(modulename, filename)
  
  # Try finding module for some other source (unsupported)
  return None

# Finds a `.java` file with `filename`, or `None` when not found
def lookup(filename: str) -> JavaFileMapping | None:
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
