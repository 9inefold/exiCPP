import sys, traceback
from pathlib import Path

EXI_BASE_FOLDER = Path("C:/Users/alex/Documents/GitHub/exiCPP")

import _jpype
_jpype.enableStacktraces(True)

import jpype
import jpype.imports
from jpype.types import *

jpype.startJVM(jpype.getDefaultJVMPath(), '-ea',
  classpath=[EXI_BASE_FOLDER.as_posix() + '/bin/*'],
  convertStrings=False)

if not jpype.isJVMStarted():
  print("JVM is not running!")

from java.io import FileInputStream, InputStream, ByteArrayInputStream, ByteArrayOutputStream, StringWriter, FileWriter
from java.lang import String
from com.exicpp.openexi import LoggingSAXHandlerWrapper, CustomSAXParserFactory
#from org.openexi.scomp import EXISchemaReader
from org.openexi.schema import EXISchema, EmptySchema
from org.openexi.sax import Transmogrifier, EXIReader
from org.openexi.proc import HeaderOptionsOutputType
from org.openexi.proc.common import AlignmentType, GrammarOptions, SchemaId
from org.openexi.proc.grammars import GrammarCache
from org.xml.sax import InputSource
from java.nio.charset import Charset
from javax.xml import XMLConstants
from javax.xml.transform.sax import SAXTransformerFactory
from javax.xml.transform.stream import StreamResult
from javax.xml.parsers import SAXParserFactory

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

  writer = Transmogrifier()
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
  handler = LoggingSAXHandlerWrapper(transformer_handler)

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
    try:
      w = MessageHandler.writer
      input = ByteArrayInputStream(contents.getBytes(Charset.forName("utf8")));
      output = ByteArrayOutputStream();
      #t.setGrammarCache(MessageHandler.grammar_cache, MessageHandler.schemaid);
      w.setGrammarCache(GrammarCache(None, MessageHandler.gmr_options));
      w.setOutputStream(output);
      w.encode(InputSource(input));
      result = output.toByteArray()
    except:
      print(traceback.format_exc())
    finally:
      if input:
        input.close()
      if output:
        output.close()
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
      tf_handler = MessageHandler.transformer_handler
      #r.setGrammarCache(MessageHandler.grammar_cache, MessageHandler.schemaid);
      r.setGrammarCache(MessageHandler.grammar_cache);
      tf_handler.setResult(StreamResult(stringWriter))
      r.parse(InputSource(input))
      result = stringWriter.getBuffer().toString()
    except:
      print(traceback.format_exc())
    finally:
      if input:
        input.close()
      if output:
        output.close()
      return str(result)

if __name__ == "__main__":
  CustomSAXParserFactory.printTypeOfParser()

  xml_in = (EXI_BASE_FOLDER / 'tests/s/xml/042.xml').read_text('utf8')
  data = MessageHandler.encode(xml_in)
  (EXI_BASE_FOLDER / 'tests/042.exi').write_bytes(data)
  xml_out = MessageHandler.decode(data)
  print(xml_out)
  pass
