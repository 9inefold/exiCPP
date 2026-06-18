import os, re, tempfile, traceback, subprocess
from pathlib import Path
from exiconf.constants import EXICPP_EXECUTABLE, EXI_BIN_DIR
#from exiconf.coder import ExiOptions, PreserveType, AlignmentType as PyAlignmentType
from exiconf.base_coder import BaseCoder
from exiconf.logging import Log, LogLevel

__all__ = ['ExicppCoder']

EXICPP_VERBOSITY = ''
def set_exicpp_verbosity(val: str):
  global EXICPP_VERBOSITY
  EXICPP_VERBOSITY = val

ENVIRON = { k: v for k, v in os.environ.items() }
ENVIRON['EXICPP_NO_ANSI'] = '1'

FAIL_KIND = [
  'Success',
  'Early exit',
  'Failed during cl parsing, file not found, etc.',
  'Failed while parsing input xml/exi',
  'Failed during conversion',
  'Failed after everything else',
]

def _get_fail_kind(val: int) -> str:
  if val < len(FAIL_KIND):
    return f'{val}: {FAIL_KIND[val]}'
  # Check os specific codes
  if os.name == 'nt':
    return f'0x{val:08X}: Unknown' 
  return f'{val}: Unknown'

# There are more end sequences, but I won't be including them.
ANSI_PATTERN = r'(\x1B\[\d{1,3}(?:;\d{1,3}){,4}m)'
ANSI_FRONT = re.compile(ANSI_PATTERN + r'\s*') # Front
ANSI_BACK = re.compile(r'\s*' + ANSI_PATTERN + '$') # Back

# Try ' \x1B[0m  \n\x1B[1;31m  abc \x1B[0m xyz \x1B[33m \x1B[1;31m\n '
def _strip_ansi(output: str, logger: Log, keep_back: bool = False) -> str:
  stripped = output.strip()
  if not logger.color_enabled:
    return stripped

  # Strip ANSI codes on the front
  front = []
  while stripped.startswith('\x1B['):
    m = ANSI_FRONT.match(stripped)
    if m is None:
      raise ValueError('unterminated escape sequence')
    front.append(m.group(1))
    stripped = stripped[m.end():]
  
  # Strip ANSI codes from the back
  back = []
  while stripped.endswith('m'):
    m = ANSI_BACK.search(stripped)
    if m is None:
      break
    elif m.start() == 0:
      raise ValueError('uncleared starting ansi sequence?')
    elif keep_back:
      back.insert(0, m.group(1))
    stripped = stripped[:m.start()]
  
  # Recombine
  if keep_back:
    return ''.join(front) + stripped + ''.join(back)
  else:
    return ''.join(front) + stripped

def _run_process(args: list[str], logger: Log):
  env = None if logger.color_enabled else ENVIRON
  return subprocess.run(args, timeout=5,
                        cwd=EXI_BIN_DIR, env=env,
                        capture_output=True, text=True)

def _run_coder(args: list[str], logger: Log) -> bool:
  args = [EXICPP_EXECUTABLE.as_posix(), *args]
  result = _run_process(args, logger)
  if result.returncode == 0:
    return True
  # Log stuff
  mangled = args[1]
  name = Path(args[2]).stem
  kind = _get_fail_kind(result.returncode)
  try:
    logger.error(f'{name}/{mangled} ({kind}):')
    logger.extra(f"{' '.join(args)}:")
    if logger.level >= LogLevel.EXTRA:
      stdout = _strip_ansi(result.stdout, logger)
      stderr = _strip_ansi(result.stderr, logger)
      if len(stdout) != 0:
        logger.warn()
        logger.warn(stdout, flush=True)
      if len(stderr) != 0:
        logger.error()
        logger.error(stderr, flush=True)
  except:
    pass
  return False

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

# TODO: Implement encode/decode?
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
      args.append('-V' + EXICPP_VERBOSITY)
    if not self.logger.color_enabled:
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
