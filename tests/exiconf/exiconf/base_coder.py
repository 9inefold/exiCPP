from pathlib import Path
from exiconf.coder import ExiOptions
from exiconf.logging import Log, outs, errs

__all__ = ['BaseCoder']

_default_sig = 'yPcdip'

class BaseCoder(ExiOptions):
  __slots__ = ('mangled', 'logger',)
  mangled: str
  logger: Log

  def __init__(self, mangled=None, logger=None):
    if logger is None:
      logger = outs()
    else:
      assert isinstance(logger, Log)
    
    if mangled is None:
      logger.extra(f"mangled is None, using '{_default_sig}'")
      mangled = _default_sig
    else:
      mangled = mangled[:]
    
    super().__init__(mangled)
    self.mangled = mangled
    self.logger = logger

  # TODO: Remove dir
  def encode(self, xml_contents: str, filename=None, dir=None) -> bytes | None:
    raise NotImplementedError("encode is not implemented!")

  def decode(self, exi_contents: bytes, filename=None, dir=None) -> str | None:
    raise NotImplementedError("decode is not implemented!")

  def encode_file(self, xml_in: Path, exi_out: Path) -> bool:
    if not self.check_encode_files(xml_in, exi_out):
      return False
    # Handle file loading
    xml_contents = xml_in.read_text('utf8')
    encoded = self.encode(xml_contents, filename=xml_in.name)
    if encoded is None:
      return False
    # Write output
    exi_out.write_bytes(encoded)
    return True

  def decode_file(self, exi_in: Path, xml_out: Path, /) -> bool:
    if not self.check_decode_files(exi_in, xml_out):
      return False
    # Handle file loading
    exi_contents = exi_in.read_bytes()
    decoded = self.decode(exi_contents, filename=exi_in.name)
    if decoded is None:
      return False
    # Write output
    xml_out.write_text(decoded, encoding='utf8', newline='\n')
    return True
  
  def check_encode_files(self, xml_in: Path, exi_out: Path, /) -> bool:
    # Check existence
    if not xml_in.exists():
      self.logger.error(f"input '{xml_in.as_posix()}' does not exist!")
      return False
    if not exi_out.parent.exists():
      self.logger.error(f"output '{exi_out.as_posix()}' folder does not exist!")
      return False
    # Check extensions
    if xml_in.suffix != '.xml':
      self.logger.warn(f"input file suffix '{xml_in.suffix}' is not .xml")
    if exi_out.suffix != '.exi':
      self.logger.warn(f"output file suffix '{exi_out.suffix}' is not .exi")
    return True
  
  def check_decode_files(self, exi_in: Path, xml_out: Path) -> bool:
    # Check existence
    if not exi_in.exists():
      self.logger.error(f"'{exi_in.as_posix()}' does not exist!")
      return False
    if not xml_out.parent.exists():
      self.logger.error(f"output '{xml_out.as_posix()}' folder does not exist!")
      return False
    # Check extensions
    if exi_in.suffix != '.exi':
      self.logger.warn(f"input file suffix '{exi_in.suffix}' is not .exi")
    if xml_out.suffix != '.xml':
      self.logger.warn(f"output file suffix '{xml_out.suffix}' is not .xml")
    return True
