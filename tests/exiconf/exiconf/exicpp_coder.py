import os, tempfile, traceback, subprocess
from pathlib import Path
from exiconf.constants import EXICPP_EXECUTABLE, EXI_BIN_DIR
#from exiconf.coder import ExiOptions, PreserveType, AlignmentType as PyAlignmentType
from exiconf.base_coder import BaseCoder
from exiconf.logging import Log, LogLevel

__all__ = ['ExicppCoder']

FAIL_KIND = [
  'Success',
  'Failed during cl parsing, file not found, etc.',
  'Failed while parsing input xml/exi',
  'Failed during conversion',
  'Failed after everything else',
]

class TempFile:
  __slots__ = ('_path', '_tmp',)
  _path: Path
  _tmp: bool

  def __init__(self, filename, outpath):
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

def _get_fail_kind(val: int) -> str:
  if val < len(FAIL_KIND):
    return FAIL_KIND[val]
  # TODO: Check for windows error?
  return 'Unknown'

def _run_process(args: list[str]):
  return subprocess.run(args,
                        cwd=EXI_BIN_DIR, capture_output=True, text=True)

def _run_coder(args: list[str], logger: Log) -> bool:
  result = _run_process([EXICPP_EXECUTABLE.as_posix(), *args])
  if result.returncode == 0:
    return True
  # Log stuff
  mangled = args[1]
  name = Path(args[2]).stem
  kind = _get_fail_kind(result.returncode)
  try:
    logger.error(f'{name}/{mangled} ({result.returncode}: {kind}):')
    #logger.extra(f"{args}:")
    stdout = result.stdout.strip(' \t\r\n')
    stderr = result.stderr.strip(' \t\r\n')
    # TODO: Strip escape sequences
    if len(stdout) != 0:
      logger.extra(stdout, flush=True)
    if len(stderr) != 0:
      logger.extra(stderr, flush=True)
  except:
    pass
  return False

# TODO: Implement encode/decode
class ExicppCoder(BaseCoder):
  def __init__(self, mangled=None, logger=None):
    super().__init__(mangled, logger)

  def encode(self, xml_contents: str, filename=None, dir=None) -> bytes | None:
    result = None
    f = TempFile(filename, dir)
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

  def decode(self, exi_contents: bytes, filename=None, dir=None) -> str | None:
    result = None
    f = TempFile(filename, dir)
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
  
  def _code_file_common(self, mode: str, fin: str, fout: str) -> bool:
    args = [mode, self.mangled, fin, fout]
    if self.logger.level == LogLevel.EXTRA:
      args.append('-V')
    #if self.logger.color_enabled:
    args.append('-T')
    # Run the command
    return _run_coder(args, self.logger)
  
  def encode_file(self, xml_in: Path, exi_out: Path) -> bool:
    if not super().check_encode_files(xml_in, exi_out):
      return False
    # Set up subprocess command
    return self._code_file_common('e', xml_in.as_posix(), exi_out.as_posix())

  def decode_file(self, exi_in: Path, xml_out: Path, /) -> bool:
    if not super().check_decode_files(exi_in, xml_out):
      return False
    # Set up subprocess command
    return self._code_file_common('d', exi_in.as_posix(), xml_out.as_posix())
