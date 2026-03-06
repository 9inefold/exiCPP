import traceback
from pathlib import Path
from exiconf.jvm import do_jvm_check, log_jexception
do_jvm_check(__file__)

from exiconf.coder import ExiOptions, PreserveType, AlignmentType as PyAlignmentType
from exiconf.logging import outs, errs
from exiconf.jvm import *

import jpype.imports
from jpype.types import *
from java.io import ByteArrayInputStream, ByteArrayOutputStream, StringWriter
from java.lang import String
from org.exicpp.openexi import DirectSAXHandler
from org.openexi.sax import EXIReader2, Transmogrifier2
from org.openexi.proc import HeaderOptionsOutputType
from org.openexi.proc.common import AlignmentType, GrammarOptions
from org.openexi.proc.grammars import GrammarCache
from org.xml.sax import InputSource
from java.nio.charset import Charset
from javax.xml.transform.stream import StreamResult

__all__ = ['OpenEXICoder']

_default_sig = 'yPcdip'

def get_alignment(align: PyAlignmentType):
  match align:
    case align.BitPacked:
      return AlignmentType.bitPacked
    case align.BytePacked:
      return AlignmentType.byteAligned
    case align.PreCompression:
      return AlignmentType.preCompress
    case align.Compression:
      return AlignmentType.compress
    case _:
      raise ValueError(f'invalid align {repr(align)}')

def get_options(preserve: PreserveType):
  options = GrammarOptions.DEFAULT_OPTIONS
  if not preserve:
    return options
  # Handle options
  if preserve & preserve.Comments:
    options = GrammarOptions.addCM(options)
  if preserve & preserve.DTDs:
    options = GrammarOptions.addDTD(options)
  if preserve & preserve.LexicalValues:
    pass
  if preserve & preserve.PIs:
    options = GrammarOptions.addPI(options)
  if preserve & preserve.Prefixes:
    options = GrammarOptions.addNS(options)
  return options

class OpenEXICoder(ExiOptions):
  """
  The following is modified from:
    https://github.com/EDF-Lab/eVDriveFlow/blob/main/shared/message_handling.py
  This is the class that will process every single XML input.
  """

  # TODO Add demangling settings
  def __init__(self, mangled=_default_sig, logger=outs):
    if mangled is None:
      logger.extra(f"mangled is None, using '{_default_sig}'")
      self.mangled = _default_sig
    else:
      self.mangled = mangled
    super().__init__(self.mangled)

    self.logger = logger
    self.hdr_options = HeaderOptionsOutputType.none
    self.align = get_alignment(self.Alignment)
    self.gmr_options = get_options(self.Preserve)

    if self.SchemaID:
      logger.warn(f"SchemaID set to '{self.SchemaID}', but schemas are currently disabled.")
    # TODO: Allow schemas?
    self.schema = None

    writer = Transmogrifier2()
    writer.setAllowWeirdAttributes(True)
    writer.setPreserveCharacterRefEmbedding(True)
    writer.setResolveExternalGeneralEntities(JBoolean(False))
    writer.setPreserveWhitespaces(JBoolean(True))
    #writer.printXMLReaderConfig()
    writer.setOutputOptions(self.hdr_options)
    writer.setAlignmentType(self.align)
    if self.BlockSize >= 0:
      writer.setBlockSize(self.BlockSize)
    if self.ValueMaxLength >= 0:
      writer.setValueMaxLength(self.ValueMaxLength)
    if self.ValuePartitionCapacity >= 0:
      writer.setValuePartitionCapacity(self.ValuePartitionCapacity)

    reader = EXIReader2()
    handler = DirectSAXHandler()
    reader.setAlignmentType(self.align)
    #reader.setOutputOptions(OpenEXICoder.hdr_options)
    #reader.setResolveExternalGeneralEntities(JBoolean(False))
    ##reader.setBlockSize(1000000)
    ##reader.setValueMaxLength(-1)
    ##reader.setValuePartitionCapacity(0)
    reader.setContentHandler(handler)
    reader.setLexicalHandler(handler)

    self.writer = writer
    self.reader = reader
    self.handler = handler
  
  @property
  def grammar_cache(self):
    return GrammarCache(self.schema, self.gmr_options)

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
      w.setGrammarCache(self.grammar_cache);
      w.setOutputStream(output);
      w.encode(InputSource(input));
      result = output.toByteArray()
      w.resetEntityManagerSearchDirectories()
    except Exception as e:
      if filename is not None:
        self.logger.error(Path(filename).as_posix(), ':', sep='')
      if isinstance(e, JException):
        log_jexception(e, logger=self.logger)
      else:
        self.logger.error(traceback.format_exc())
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
      r.setGrammarCache(self.grammar_cache);
      r.parse(InputSource(input))
      result = stringWriter.getBuffer().toString()
      self.handler.reset()
    except Exception as e:
      if filename is not None:
        print(Path(filename).as_posix(), ':', sep='')
      if isinstance(e, JException):
        log_jexception(e, logger=self.logger)
      else:
        self.logger.error(traceback.format_exc())
      pass
    finally:
      if input:
        input.close()
      if output:
        output.close()
      if result is None:
        return None
      return str(result)
