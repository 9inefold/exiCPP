import os, sys
from pathlib import Path
from glob import glob
from exiconf.main import EXI_BASE_DIR
from exiconf.logging import errs
from exiconf.jvm.setup import get_jvm_path

class JavaFileMapping:
  """
  self.is_multi: boolean
  """
  def __init__(self, is_multi=False):
    self.is_multi = is_multi
  def get_line(self, lineno: int) -> str:
    return ''
  def get_line_ext(self, clazzname: str, lineno: int) -> str:
    return self.get_line(lineno)

# Maps lines in a JException to java files
class JavaRealFileMapping(JavaFileMapping):
  """
  self.__package: Path
  self.__filename: Path
  self.__filedata: list[str]?
  """

  def __init__(self, package: str, filename: Path):
    super().__init__()
    self.__package = str(package)
    self.__filename = Path(filename)
    self.__filedata = None

  @property
  def package(self) -> str:
    return self.__package

  @property
  def filename(self) -> str:
    return self.__filename.as_posix()

  @property
  def cache(self) -> list[str]:
    cache = self.__filedata
    if cache is not None:
      return cache
    # Load the file data
    try:
      blob = self.__filename.read_text(encoding='utf8')
      lines = blob.splitlines(keepends=True)
      if lines and not lines[-1].endswith('\n'):
        lines[-1] += '\n'
      self.__filedata = lines
      return lines
    except (OSError, UnicodeDecodeError):
      # print('failure!')
      self.__filedata = []
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
  """
  self.__mappings: list[JavaRealFileMapping]
  self.__mappingdict: dict[str, JavaRealFileMapping]
  """

  def __init__(self, mappings: list[JavaRealFileMapping] = None):
    super().__init__(is_multi=True)
    if mappings is None:
      self.__mappings = []
    else:
      assert isinstance(mappings, list)
      self.__mappings = mappings[:]
    # Set up dict
    md = {}
    for mapping in self.__mappings:
      pkg = mapping.package
      if pkg in self.__mappingdict:
        errs.warn(f"Conflicting package '{pkg}' in JavaMultiFileMapping?")
        continue
      md[pkg] = mapping
    self.__mappingdict = md
  
  def add(self, mapping: JavaRealFileMapping):
    if mapping is None:
      return
    if not isinstance(mapping, JavaRealFileMapping):
      errs.warn(f'{type(mapping)} passed to JavaMultiFileMapping?')
      return
    pkg = mapping.package
    if pkg in self.__mappingdict:
      errs.warn(f"Conflicting package '{pkg}' in JavaMultiFileMapping?")
      return
    self.__mappings.append(mapping)
    self.__mappingdict[pkg] = mapping

  def get_line(self, lineno: int) -> str:
    if len(self.__mappings) == 0:
      return ''
    # Always use the first if there is no way to disambiguate
    return self.__mappings[0].get_line(lineno)
  
  def get_line_ext(self, clazzname: str, lineno: int) -> str:
    modulename, clazzname = clazzname.rsplit('.', 1)
    ppath = modulename.replace('.', '/')
    md = self.__mappingdict
    if ppath in md:
      return md[ppath].get_line(lineno)
    return ''

# Wraps behaviour for lines in a zipped file.
class JavaZipFileMapping(JavaFileMapping):
  """
  self.__filedata: list[str]
  """
  
  def __init__(self, filedata: list[str]):
    super().__init__()
    assert filedata is not None
    self.__filedata = filedata
  
  @property
  def cache(self) -> list[str]:
    return self.__filedata

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
  """
  self.__zip: zipfile.ZipFile
  self.__names: list[Text]
  self.__cache: dict[str, list[str]?]
  """
  
  def __init__(self, _zip):
    self.__zip = _zip
    if self.__zip is not None:
      self.__names = _zip.namelist()
    else:
      self.__names = None
    self.__cache = {}
  
  def __lookup(self, matches) -> bytes:
    zf = self.__zip
    for match in matches:
      try:
        return zf.read(match)
      except KeyError:
        pass
    return None

  def lookup(self, modulename: str, filename: str) -> JavaZipFileMapping:
    if self.__zip is None:
      return None
    if modulename in self.__cache:
      return self.__cache[modulename]
    # Get the relative path
    modulepath = modulename.replace('.', '/')
    modulepath += f'/{filename}'
    matches = [x for x in self.__names if x.endswith(modulepath)]
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
      self.__cache[modulename] = m
      return m
    except (OSError, UnicodeDecodeError):
      self.__cache[modulename] = None
    return None

_java_file_line_dict: dict[str, JavaFileMapping]
_java_file_line_dict = None

_java_src_zip: JavaSrcZip
_java_src_zip = None
_java_src_zip_tried_load = False

# Finds the JDK 'src.zip' file containing the sources.
def _find_java_src() -> Path:
  jvm_path = get_jvm_path()
  if jvm_path is None:
    errs.error('JVM was not started!')
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
      errs.info(f"unable to find 'src.zip'")
      _java_src_zip = JavaSrcZip(None)
      return None
    zf = zipfile.ZipFile(str(src_zip), 'r')
    _java_src_zip = JavaSrcZip(zf)
    #print(zf.namelist())
    return _java_src_zip
  except Exception as e:
    errs.info(f"unable to load 'src.zip': {e}")
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
def lookup_ext(clazzname, filename: str) -> JavaFileMapping:
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
def lookup(filename: str) -> JavaFileMapping:
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
