import os, re, sys, traceback
from pathlib import Path
from glob import glob

EXI_CURR_DIR = Path(__file__).parent
sys.path.insert(0, str(EXI_CURR_DIR / 'exiconf'))
from exiconf.main import EXI_BASE_DIR, EXI_BIN_DIR, TEST_SRC_DIR

sys.path.insert(0, str(EXI_BASE_DIR / 'vendored' / 'xmldiff'))
from xmldiff.main import diff_texts
from xmldiff.actions import UpdateTextIn, UpdateTextAfter
import lxml.etree as etree

# Returns true if xml is not different.
def diff_xml(name, file1, file2, parse_options=None) -> bool:
  try:
    bytes1 = str(file1).encode()
    bytes2 = str(file2).encode()
    diff_list = diff_texts(bytes1, bytes2, parse_options=parse_options)
    real_diffs = []
    # Fixup diffs with empty data
    # TODO: Add option to compare without preserves
    for diff in diff_list:
      if isinstance(diff, (UpdateTextIn, UpdateTextAfter)):
        old = diff.oldtext if diff.oldtext is not None else ''
        new = diff.text if diff.text is not None else ''
        if old.strip() != new.strip():
          real_diffs.append(diff)
      else:
        real_diffs.append(diff)
    if len(real_diffs) != 0:
      print(f'[diff] {Path(name).as_posix()}: {diff_list}')
      return False
    return True
  except Exception as e:
    print(f'[diff] {Path(name).as_posix()}*: {type(e).__name__}: {e}')
    return False

cwd = os.getcwd()

import _jpype
_jpype.enableStacktraces(True)

import jpype
import jpype.imports
from jpype.types import *

jpype_jvmpath = jpype.getDefaultJVMPath()
jpype.startJVM(jpype_jvmpath, '-ea',
  '--add-opens=java.base/java.lang.reflect=ALL-UNNAMED',
  '--add-opens=java.xml/com.sun.org.apache.xalan.internal.xsltc.trax=ALL-UNNAMED',
  '--add-opens=java.xml/com.sun.org.apache.xalan.internal.xsltc.runtime=ALL-UNNAMED',
  #'--add-exports=java.base/jdk.internal.vm.annotation=ALL-UNNAMED',
  #'-Dexicpp.loglevel=verbose',
  classpath=[
    EXI_BASE_DIR.as_posix() + '/bin/*',
    EXI_BIN_DIR.as_posix() + '/*'
  ],
  convertStrings=False)

if not jpype.isJVMStarted():
  print("JVM is not running!")

class JavaZipFileMapping:
  # self.__filedata: list[str]

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

class JavaSrcZip:
  # self.__zip: zipfile.ZipFile
  # self.__names
  # self.__cache: dict[str, list[str]?]
  
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

class JavaFileMapping:
  # self.__filename: Path
  # self.__filedata: list[str]?

  _java_file_line_dict: dict[str, 'JavaFileMapping']
  _java_src_zip: JavaSrcZip

  # TODO: Add a way to embed java mappings in their jar files
  _java_file_line_dict = None
  _java_src_zip = None

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
    if JavaFileMapping._java_file_line_dict is not None:
      return JavaFileMapping._java_file_line_dict
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
    JavaFileMapping._java_file_line_dict = file_line_dict
    return JavaFileMapping._java_file_line_dict

  @staticmethod
  def __find_java_src() -> Path:
    java_home = Path(jpype_jvmpath).parent
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

  @staticmethod
  def __java_src() -> JavaSrcZip:
    if JavaFileMapping._java_src_zip is not None:
      return JavaFileMapping._java_src_zip
    try:
      import zipfile
      src_zip = JavaFileMapping.__find_java_src()
      if src_zip is None:
        JavaFileMapping._java_src_zip = JavaSrcZip(None)
        return None
      zf = zipfile.ZipFile(str(src_zip), 'r')
      JavaFileMapping._java_src_zip = JavaSrcZip(zf)
      #print(zf.namelist())
      return JavaFileMapping._java_src_zip
    except Exception as e:
      return None
  
  @staticmethod
  def is_in_std_java_module(clazzname) -> bool:
    if clazzname is None:
      return False
    return clazzname.startswith((
      "java.", "javax.", "jdk.", "sun.", "com.sun."))

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
    if not JavaFileMapping.is_in_std_java_module(clazzname):
      return None
    modulename, _clazzname = clazzname.rsplit('.', 1)
    # Try finding module for some other source
    src_zip = JavaFileMapping.__java_src()
    if src_zip is None:
      return None
    return src_zip.lookup(modulename, filename)
  
  @staticmethod
  def lookup_line(filename: str, lineno: int) -> str:
    m = JavaFileMapping.lookup_ext(None, filename)
    if m is None:
      return ''
    return m.get_line(lineno)
  
  @staticmethod
  def lookup_line_ext(clazz: str, filename: str, lineno: int) -> str:
    m = JavaFileMapping.lookup_ext(clazz, filename)
    if m is None:
      return ''
    return m.get_line(lineno)
  
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

from java.io import FileInputStream, InputStream, ByteArrayInputStream, ByteArrayOutputStream, StringWriter, FileWriter
from java.lang import String
from org.exicpp.openexi import DirectSAXHandler
from org.openexi.schema import EXISchema, EmptySchema
from org.openexi.sax import EXIReader2, Transmogrifier2
from org.openexi.proc import HeaderOptionsOutputType
from org.openexi.proc.common import AlignmentType, GrammarOptions, SchemaId
from org.openexi.proc.grammars import GrammarCache
from org.xml.sax import InputSource
from java.nio.charset import Charset
from javax.xml import XMLConstants
from javax.xml.transform.sax import SAXTransformerFactory
from javax.xml.transform.stream import StreamResult
from javax.xml.parsers import SAXParserFactory

def format_jexception_like_py(ex: JException) -> list[str]:
  found_nonerr = False
  frames = ex.getStackTrace()
  out = []
  for frame in frames:
    _method = str(frame.getMethodName())
    _clazz = str(frame.getClassName())
    # Skip reportError and friends
    if not found_nonerr:
      if _clazz.startswith('org.apache.xerces.'):
        if re.fullmatch("report([A-Z][a-z]+)?Error", _method):
          continue
      found_nonerr = True

    to_push = '  File '
    _file = frame.getFileName()
    _line = frame.getLineNumber()
    _line_data = ''
    if _file:
      to_push += f'"{_file}"'
    else:
      to_push += "<unknown>"
    if _line > 0:
      to_push += f', line {_line}'
      try:
        _line_data = JavaFileMapping.lookup_line_ext(_clazz, _file, _line)
      except Exception as e:
        #print("{{")
        #traceback.print_tb(e.__traceback__)
        #print(e)
        #print("}}", flush=True)
        pass
    to_push += f', in {_clazz}.{_method}\n'
    if len(_line_data) > 0:
      to_push += f'    {_line_data}\n'
    out.append(to_push)
    # TODO: Add source?
    pass
  return list(reversed(out))

def print_jexc(ex: JException):
  stacks = traceback.format_stack()[:-2]
  stacks.extend(traceback.format_tb(ex.__traceback__))
  stacks.extend(format_jexception_like_py(ex))
  print("Traceback (most recent call last):\n",
        ''.join(stacks), f'{ex.toString()}\n',
        sep='', flush=True)

def get_full_options():
  options = GrammarOptions.DEFAULT_OPTIONS
  options = GrammarOptions.addCM(options)
  options = GrammarOptions.addPI(options)
  options = GrammarOptions.addNS(options)
  options = GrammarOptions.addDTD(options)
  return options

"""
The following class is modified from:
https://github.com/EDF-Lab/eVDriveFlow/blob/main/shared/message_handling.py
"""

class MessageHandler:
  """This is the class that will process every single XML input."""
  hdr_options = HeaderOptionsOutputType.none
  gmr_options = get_full_options()

  def __init__(self):
    writer = Transmogrifier2()
    #writer.printXMLReaderConfig()
    writer.setAllowWeirdAttributes(True)
    writer.setPreserveCharacterRefEmbedding(True)
    writer.setOutputOptions(MessageHandler.hdr_options)
    writer.setAlignmentType(AlignmentType.byteAligned)
    writer.setResolveExternalGeneralEntities(JBoolean(False))
    writer.setPreserveWhitespaces(JBoolean(True))
    #writer.setBlockSize(1000000)
    #writer.setValueMaxLength(-1)
    #writer.setValuePartitionCapacity(0)

    reader = EXIReader2()
    handler = DirectSAXHandler()
    reader.setAlignmentType(AlignmentType.byteAligned)
    #reader.setOutputOptions(MessageHandler.hdr_options)
    #reader.setResolveExternalGeneralEntities(JBoolean(False))
    ##reader.setBlockSize(1000000)
    ##reader.setValueMaxLength(-1)
    ##reader.setValuePartitionCapacity(0)
    reader.setContentHandler(handler)
    reader.setLexicalHandler(handler)

    self.writer = writer
    self.reader = reader
    self.handler = handler
    self.hdr_options = MessageHandler.hdr_options
    self.gmr_options = MessageHandler.gmr_options

  def encode(self, xml_contents: str, filename=None, dir=None) -> bytes:
    """Turns a human-readable string to an EXI-encoded string. Relies on Java classes.

    :param xml_contents: The XML string to be encoded.
    :param type_msg: The type of message used.
    :return: str -- the encoded result.
    """
    contents = String(xml_contents)
    input = None
    output = None
    result = None
    try:
      w = self.writer
      input = ByteArrayInputStream(contents.getBytes(Charset.forName("utf8")));
      output = ByteArrayOutputStream();
      if dir is not None:
        w.addEntityManagerSearchDirectory(Path(dir).as_posix())
      w.setGrammarCache(GrammarCache(None, self.gmr_options));
      w.setOutputStream(output);
      w.encode(InputSource(input));
      result = output.toByteArray()
      w.resetEntityManagerSearchDirectories()
    except Exception as e:
      if filename is not None:
        print(Path(filename).as_posix(), ':', sep='')
      if isinstance(e, JException):
        print_jexc(e)
      else:
        traceback.print_exc()
      pass
    finally:
      if input:
        input.close()
      if output:
        output.close()
      if result is None:
        return None
      return result

  def decode(self, exi_contents: bytes, filename=None) -> str:
    """Turns encoded EXI bytes to human-readable string. Relies on Java classes.

    :param exi_contents: The EXI encoded contents.
    :param type_msg: The type of message used.
    :return: str -- the decoded string.
    """
    input = None
    output = None
    stringWriter = StringWriter()
    result = None
    try:
      input = ByteArrayInputStream(exi_contents)
      r = self.reader
      self.handler.setWriter(stringWriter)
      r.setGrammarCache(GrammarCache(None, self.gmr_options));
      r.parse(InputSource(input))
      result = stringWriter.getBuffer().toString()
      self.handler.reset()
    except Exception as e:
      if filename is not None:
        print(Path(filename).as_posix(), ':', sep='')
      if isinstance(e, JException):
        print_jexc(e)
      else:
        traceback.print_exc()
      pass
    finally:
      if input:
        input.close()
      if output:
        output.close()
      if result is None:
        return None
      return str(result)

OUT_DIR = EXI_BASE_DIR / 'tests/out'
if not OUT_DIR.exists():
  OUT_DIR.mkdir()

BAD_ITEMS = [str(Path(x)) for x in ['xml/012.xml']]

def do_roundtrip(test_path: Path, do_print = False):
  handler = MessageHandler()
  if do_print:
    relpath = test_path.relative_to(cwd).as_posix()
    print(relpath, ':', sep='', flush=True);
  test_path = Path(test_path)
  stem = str(test_path.stem)
  xml_in = test_path.read_text('utf8')
  data = handler.encode(xml_in)
  if data is None:
    return
  (OUT_DIR / f'{stem}.exi').write_bytes(data)
  xml_out = handler.decode(data)
  if xml_out is None:
    return
  (OUT_DIR / f'{stem}.xml').write_bytes(xml_out.encode('utf8'))
  if do_print:
    print(xml_out, '\n', flush=True)

def run_all_files(do_print = False):
  all_files = list(glob(
    '**/*.xml',
    #'at/*.xml',
    root_dir=TEST_SRC_DIR,
    recursive=True
  ))

  handler = MessageHandler()

  err_dir = EXI_CURR_DIR/'out/err'
  if not err_dir.exists():
    err_dir.mkdir(parents=True)
  else:
    err_files = glob('*.xml', root_dir=err_dir)
    for f in err_files:
      os.remove(str(err_dir/f))

  eq_count = 0
  for f in all_files:
    is_eq = False
    is_bad = f in BAD_ITEMS
    xml_out = None
    try:
      xml_dir = Path(TEST_SRC_DIR / f).parent
      xml_in = (TEST_SRC_DIR / f).read_text('utf8')
      data = handler.encode(xml_in, f, xml_dir)
      if data is None:
        continue
      xml_out = handler.decode(data, f)
      if xml_out is None:
        continue
      if not is_bad:
        is_eq = diff_xml(f, xml_in, xml_out)
        # Print results
        if is_eq and do_print:
          print(f'{Path(f).as_posix()}: {is_eq}', flush=True)
      elif do_print:
        print(f'{Path(f).as_posix()}: Skipped', flush=True)
        
    except Exception as e:
      if do_print:
        print(f'{Path(f).as_posix()}*: {type(e).__name__}: {e}', flush=True)
      pass
    # Check results
    if is_eq:
      eq_count += 1
    elif xml_out is not None:
      if is_bad:
        # Skipping, but add to the count anyways
        eq_count += 1
      (EXI_CURR_DIR/'out/err'/Path(f).name).write_text(xml_out)
  
  print('Equal:', eq_count)
  print('Total:', len(all_files))

if __name__ == "__main__":
  #run_all_files(do_print=True)
  run_all_files()

  #CustomSAXParserFactory.printTypeOfParser()
  # TODO: Handle doctype
  
  #do_roundtrip(TEST_SRC_DIR / 'xml/008.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'xml/008r.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'xml/042.xml')
  #do_roundtrip(TEST_SRC_DIR / 'xml/066.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'xml/080.xml', do_print=True)

  #do_roundtrip(TEST_SRC_DIR / 'xml/070.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'at/at-01.xml', do_print=True)

  #do_roundtrip(TEST_SRC_DIR / 'me/HTML.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'me/HTML2.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'me/HTML3.xml', do_print=True)

  #do_roundtrip(TEST_SRC_DIR / 'me/Nested.xml')
  #do_roundtrip(TEST_SRC_DIR / 'me/CDATA2.xml')
  ##do_roundtrip(EXI_BASE_DIR / 'tests/nested-ent.hidden.xml')
  pass
