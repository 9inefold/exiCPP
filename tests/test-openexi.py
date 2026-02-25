import os, sys, traceback
from pathlib import Path

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
      print(f'{Path(name).as_posix()}: {diff_list}')
      return False
    return True
  except Exception as e:
    print(f'{Path(name).as_posix()}: {e}')
    return False

cwd = os.getcwd()

import _jpype
_jpype.enableStacktraces(True)

import jpype
import jpype.imports
from jpype.types import *

jpype.startJVM(jpype.getDefaultJVMPath(), '-ea',
  '--add-opens=java.base/java.lang.reflect=ALL-UNNAMED',
  '--add-opens=java.xml/com.sun.org.apache.xalan.internal.xsltc.trax=ALL-UNNAMED',
  '--add-opens=java.xml/com.sun.org.apache.xalan.internal.xsltc.runtime=ALL-UNNAMED',
  #'--add-exports=java.base/jdk.internal.vm.annotation=ALL-UNNAMED',
  '-Dexicpp.loglevel=verbose',
  classpath=[
    EXI_BASE_DIR.as_posix() + '/bin/*',
    EXI_BIN_DIR.as_posix() + '/*'
  ],
  convertStrings=False)

if not jpype.isJVMStarted():
  print("JVM is not running!")

from java.io import FileInputStream, InputStream, ByteArrayInputStream, ByteArrayOutputStream, StringWriter, FileWriter
from java.lang import String
from org.exicpp.openexi import DirectSAXHandler
#from org.openexi.scomp import EXISchemaReader
from org.openexi.schema import EXISchema, EmptySchema
from org.openexi.sax import EXIReader, Transmogrifier, Transmogrifier2
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
  frames = ex.getStackTrace()
  out = []
  for frame in frames:
    to_push = '  File '
    _file = frame.getFileName()
    _line = frame.getLineNumber()
    _name = f'{frame.getClassName()}.{frame.getMethodName()}'
    if _file:
      to_push += f'"{_file}"'
    else:
      to_push += "<unknown>"
    if _line > 0:
      to_push += f', line {_line}'
    out.append(f'{to_push}, in {_name}\n')
    # TODO: Add source?
    pass
  return list(reversed(out))

def print_jexc(ex: JException):
  #traceback.print_exc()
  stacks = traceback.format_stack()
  stacks.extend(format_jexception_like_py(ex))
  print("Traceback (most recent call last):\n",
        ''.join(stacks), f'{ex.toString()}: {ex.getMessage()}\n',
        sep='', flush=True)

class Singleton(type):
  """This is a singleton design pattern class."""
  _instances = {}

  def __call__(cls, *args, **kwargs):
    if cls not in cls._instances:
      cls._instances[cls] = super(Singleton, cls).__call__(*args, **kwargs)
    return cls._instances[cls]

def get_full_options():
  options = GrammarOptions.DEFAULT_OPTIONS
  options = GrammarOptions.addCM(options)
  options = GrammarOptions.addPI(options)
  options = GrammarOptions.addNS(options)
  options = GrammarOptions.addDTD(options)
  return options

class MessageHandler(metaclass=Singleton):
  """This is the class that will process every single V2GTP message."""
  hdr_options = HeaderOptionsOutputType.none
  gmr_options = get_full_options()
  schemaid = SchemaId@None
  grammar_cache = GrammarCache(None, gmr_options);

  #sax_parser_factory = SAXParserFactory.newInstance()
  #sax_parser_factory.setNamespaceAware(True)

  writer = Transmogrifier2()
  #writer.printXMLReaderConfig()
  writer.setPreserveCharacterRefEmbedding(True)
  #writer = Transmogrifier()
  writer.setOutputOptions(hdr_options)
  writer.setAlignmentType(AlignmentType.byteAligned)
  writer.setResolveExternalGeneralEntities(JBoolean(False))
  writer.setPreserveWhitespaces(JBoolean(True))
  #writer.setBlockSize(1000000)
  #writer.setValueMaxLength(-1)
  #writer.setValuePartitionCapacity(0)

  sax_transformer_factory = SAXTransformerFactory@SAXTransformerFactory.newInstance()

  #sax_transformer_factory.setAttribute("debug", JBoolean(True))
  #sax_transformer_factory.setAttribute("http://xml.org/sax/features/use-entity-resolver2", JBoolean(False))
  # http://apache.org/xml/features/xinclude

  # TODO: Implement custom Transformer Source
  transformer_handler = sax_transformer_factory.newTransformerHandler()
  #handler = LoggingSAXHandlerWrapper(transformer_handler)
  handler = DirectSAXHandler()

  reader = EXIReader()
  reader.setAlignmentType(AlignmentType.byteAligned)
  #reader.setOutputOptions(hdr_options)
  #reader.setResolveExternalGeneralEntities(JBoolean(False))
  ##reader.setBlockSize(1000000)
  ##reader.setValueMaxLength(-1)
  ##reader.setValuePartitionCapacity(0)
  reader.setContentHandler(handler)
  reader.setLexicalHandler(handler)

  def __init__(self):
    #self.parser = XmlParser(context=XmlContext())
    #self.config = SerializerConfig(pretty_print=True)
    #self.serializer = XmlSerializer(config=self.config)
    pass

  @staticmethod
  def encode(xml_contents: str) -> bytes:
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
      w = MessageHandler.writer
      input = ByteArrayInputStream(contents.getBytes(Charset.forName("utf8")));
      output = ByteArrayOutputStream();
      #t.setGrammarCache(MessageHandler.grammar_cache, MessageHandler.schemaid);
      w.setGrammarCache(GrammarCache(None, MessageHandler.gmr_options));
      w.setOutputStream(output);
      w.encode(InputSource(input));
      result = output.toByteArray()
    except Exception as e:
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

  @staticmethod
  def decode(exi_contents: bytes) -> str:
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
      r = MessageHandler.reader
      MessageHandler.handler.setWriter(stringWriter)
      #tf_handler = MessageHandler.transformer_handler
      #r.setGrammarCache(MessageHandler.grammar_cache, MessageHandler.schemaid);
      r.setGrammarCache(MessageHandler.grammar_cache);
      #tf_handler.setResult(StreamResult(stringWriter))
      r.parse(InputSource(input))
      result = stringWriter.getBuffer().toString()
      MessageHandler.handler.reset()
    except Exception as e:
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

def do_roundtrip(test_path: Path, do_print = False):
  if do_print:
    relpath = test_path.relative_to(cwd).as_posix()
    print(relpath, ':', sep='', flush=True);
  test_path = Path(test_path)
  stem = str(test_path.stem)
  xml_in = test_path.read_text('utf8')
  data = MessageHandler.encode(xml_in)
  if data is None:
    return
  (OUT_DIR / f'{stem}.exi').write_bytes(data)
  xml_out = MessageHandler.decode(data)
  if xml_out is None:
    return
  (OUT_DIR / f'{stem}.xml').write_bytes(xml_out.encode('utf8'))
  if do_print:
    print(xml_out, '\n', flush=True)

def run_all_files(do_print = False):
  from glob import glob
  all_files = list(glob(
    '**/*.xml',
    #'at/*.xml',
    root_dir=TEST_SRC_DIR,
    recursive=True
  ))

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
    xml_out = None
    try:
      xml_in = (TEST_SRC_DIR / f).read_text('utf8')
      data = MessageHandler.encode(xml_in)
      if data is None:
        continue
      xml_out = MessageHandler.decode(data)
      if xml_out is None:
        continue
      is_eq = diff_xml(f, xml_in, xml_out)
      # Print results
      if is_eq and do_print:
        print(Path(f).as_posix(), ': ', is_eq, sep='', flush=True)
      #print(xml_in)
      #print(xml_out)
    except Exception as e:
      if do_print:
        print(e, flush=True)
      pass
    # Check results
    if is_eq:
      eq_count += 1
    elif xml_out is not None:
      (EXI_CURR_DIR/'out/err'/Path(f).name).write_text(xml_out)
  
  print('Equal:', eq_count)
  print('Total:', len(all_files))

if __name__ == "__main__":
  #run_all_files(do_print=True)
  #run_all_files()

  #CustomSAXParserFactory.printTypeOfParser()
  # TODO: Handle doctype
  
  #do_roundtrip(TEST_SRC_DIR / 'xml/008.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'xml/008r.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'xml/042.xml')
  #do_roundtrip(TEST_SRC_DIR / 'xml/066.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'xml/080.xml', do_print=True)

  do_roundtrip(TEST_SRC_DIR / 'me/HTML.xml', do_print=True)
  do_roundtrip(TEST_SRC_DIR / 'me/HTML2.xml', do_print=True)
  do_roundtrip(TEST_SRC_DIR / 'me/HTML3.xml', do_print=True)

  #do_roundtrip(TEST_SRC_DIR / 'me/Nested.xml')
  #do_roundtrip(TEST_SRC_DIR / 'me/CDATA2.xml')
  ##do_roundtrip(EXI_BASE_DIR / 'tests/nested-ent.hidden.xml')
  pass
