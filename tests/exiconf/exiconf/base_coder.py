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

  def encode(self, xml_contents: str, filename=None, dir=None) -> bytes:
    self.logger.error("encode is not implemented!")
    pass

  def decode(self, exi_contents: bytes, filename=None) -> str:
    self.logger.error("decode is not implemented!")
    pass
