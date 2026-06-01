import os, tempfile, traceback
from pathlib import Path
from exiconf.coder import ExiOptions, PreserveType, AlignmentType as PyAlignmentType
from exiconf.base_coder import BaseCoder
from exiconf.logging import outs, errs

__all__ = ['ExicppCoder']

class TempFile:
  __slots__ = ('_path', '_tmp',)
  _path: Path
  _tmp: bool

  def __init__(self, filename, outpath):
    self._path = None
    self._tmp = False
    if outpath is not None:
      self._path = Path(outpath).absolute()
    else:
      #if filename is not None:
      #  filename = str(Path(filename))
      #fd, name = tempfile.mkstemp(suffix=filename, text=False)
      fd, name = tempfile.mkstemp(text=False)
      os.close(fd)
      self._path = Path(name).absolute()
      self._tmp = True

  @property
  def path(self) -> Path:
    return self._path
  
  def is_temporary(self) -> bool:
    return self._tmp

  def close(self):
    f = self._path
    if self._tmp and f.exists():
      os.unlink(str(f))


class ExicppCoder(BaseCoder):
  def __init__(self, mangled=None, logger=None):
    super().__init__(mangled, logger)

  def encode(self, xml_contents: str, filename=None, dir=None, outpath=None) -> bytes:
    result = None
    f = TempFile(filename, outpath)
    try:
      if f.is_temporary():
        self.logger.info(f'Path: {f.path.as_posix()}')
        #tmp.write(...)
    except Exception as e:
      if filename is not None:
        self.logger.error(Path(filename).as_posix(), ':', sep='')
      self.logger.error(traceback.format_exc())
    finally:
      f.close()
      return result

  def decode(self, exi_contents: bytes, filename=None, outpath=None) -> str:
    result = None
    f = TempFile(filename, outpath)
    try:
      if f.is_temporary():
        self.logger.info(f'Path: {f.path.as_posix()}')
        #tmp.write(...)
    except Exception as e:
      if filename is not None:
        self.logger.error(Path(filename).as_posix(), ':', sep='')
      self.logger.error(traceback.format_exc())
    finally:
      f.close()
      return result
