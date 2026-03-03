import os, sys
import re
import traceback
from pathlib import Path
from glob import glob
from exiconf.logging import errs
from exiconf.jvm.setup import get_jvm_path

# Wraps behaviour for lines in a zipped file.
class JavaZipFileMapping:
  """
  self.__filedata: list[str]
  """

  @property
  def cache(self) -> list[str]:
    return self.__filedata
  
  def __init__(self, filedata: list[str]):
    assert filedata is not None
    self.__filedata = filedata

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

_java_file_line_dict: dict[str, 'JavaFileMapping']
_java_src_zip: JavaSrcZip

# TODO: Add a way to embed java mappings in their jar files
_java_file_line_dict = None
_java_src_zip = None
_java_src_zip_tried_load = False

# Finds the JDK 'src.zip' file containing the sources.
def _find_java_src() -> Path:
  java_home = Path(get_jvm_path()).parent
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
      _java_src_zip = JavaSrcZip(None)
      return None
    zf = zipfile.ZipFile(str(src_zip), 'r')
    _java_src_zip = JavaSrcZip(zf)
    #print(zf.namelist())
    return _java_src_zip
  except Exception as e:
    errs.info(f"unable to load 'src.zip': {e}")
    return None

def is_class_in_std_java_module(clazzname) -> bool:
  if clazzname is None:
    return False
  return clazzname.startswith((
    "java.", "javax.", "jdk.", "sun.", "com.sun."))

# Maps lines in a JException to java files
class JavaFileMapping:
  """
  self.__filename: Path
  self.__filedata: list[str]?
  """

  @property
  def filename(self) -> str:
    return self.__filename.as_posix()
  
  @property
  def cache(self) -> list[str]:
    if self.__filedata is None:
      return self.__init_cache()
    return self.__filedata

  def __init__(self, filename: Path):
    self.__filename = Path(filename)
    self.__filedata = None
  
  @staticmethod
  def load_file_dict() -> dict[str, 'JavaFileMapping']:
    global _java_file_line_dict
    if _java_file_line_dict is not None:
      return _java_file_line_dict
    file_line_dict: dict[str, JavaFileMapping]
    file_line_dict = {}

    SEARCH_DIR = EXI_BASE_DIR/'tests/jarvis'
    all_java_files = glob(
      '**/*.java',
      root_dir=SEARCH_DIR,
      recursive=True
    )
    for java_file in all_java_files:
      jpath = SEARCH_DIR/str(java_file)
      jfile = jpath.name
      if jfile in file_line_dict:
        print(f"Warning: duplicate file {jfile}?")
        continue
      file_line_dict[jfile] = JavaFileMapping(jpath)
    _java_file_line_dict = file_line_dict
    return _java_file_line_dict

  @staticmethod
  def lookup(filename: str):
    return JavaFileMapping.lookup_ext(None, filename)

  @staticmethod
  def lookup_ext(clazzname, filename: str):
    if filename is None or len(filename) == 0:
      return None
    d = JavaFileMapping.load_file_dict()
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
  
  def __init_cache(self) -> list[str]:
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

# Calls JavaFileMapping.lookup_line
def lookup_line(filename: str, lineno: int) -> str:
  m = JavaFileMapping.lookup_ext(None, filename)
  if m is None:
    return ''
  return m.get_line(lineno)

# Calls JavaFileMapping.lookup_line_ext
def lookup_line_ext(clazz: str, filename: str, lineno: int) -> str:
  m = JavaFileMapping.lookup_ext(clazz, filename)
  if m is None:
    return ''
  return m.get_line(lineno)
