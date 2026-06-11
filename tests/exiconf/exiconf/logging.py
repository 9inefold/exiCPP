import enum
import os, sys
from io import StringIO
from exiconf.ansi import has_color as _has_color

__all__ = [
  'Color', 'Log', 'LogLevel',
  'outs', 'errs',
]

class LogLevelArgs:
  QUIET = ['quiet', 'off', 'silent', 'nothing']
  ERROR = ['error']
  WARN  = ['warn', 'warning']
  INFO  = ['info', 'note']
  EXTRA = ['extra', 'verbose']
  # Indexable names
  MAP_LEVELS = [QUIET[0], ERROR[0], WARN[0], INFO[0], EXTRA[0]]
  # List of all valid log levels
  ALL_LEVELS = [*QUIET, *ERROR, *WARN, *INFO, *EXTRA]

@enum.unique
class LogLevel(enum.IntEnum):
  QUIET = 0
  ERROR = 1
  WARN = 2
  INFO = 3
  EXTRA = 4

  @classmethod
  def create(cls, value: str):
    value = str(value).lower()
    if value in LogLevelArgs.ALL_LEVELS:
      if value == 'error':
        return cls.ERROR
      if value in LogLevelArgs.QUIET:
        return cls.QUIET
      if value in LogLevelArgs.EXTRA:
        return cls.EXTRA
      if value in LogLevelArgs.WARN:
        return cls.WARN
      if value in LogLevelArgs.INFO:
        return cls.INFO
    raise ValueError(f'invalid log level {repr(value)} of type {type(value)}')

# Represents all ansi color codes
class Color:
  BLACK           = '\x1b[30m'
  RED             = '\x1b[31m'
  GREEN           = '\x1b[32m'
  YELLOW          = '\x1b[33m'
  BLUE            = '\x1b[34m'
  MAGENTA         = '\x1b[35m'
  CYAN            = '\x1b[36m'
  WHITE           = '\x1b[37m'
  BRIGHT_BLACK    = '\x1b[90m'
  BRIGHT_RED      = '\x1b[91m'
  BRIGHT_GREEN    = '\x1b[92m'
  BRIGHT_YELLOW   = '\x1b[93m'
  BRIGHT_BLUE     = '\x1b[94m'
  BRIGHT_MAGENTA  = '\x1b[95m'
  BRIGHT_CYAN     = '\x1b[96m'
  BRIGHT_WHITE    = '\x1b[97m'
  RESET           = '\x1b[0m'

# A class used for logging
class Log:
  DEFAULT_LEVEL = LogLevel.ERROR

  __slots__ = ('level', 'color_enabled', '_file', '_has_color',)
  level: LogLevel
  color_enabled: bool
  _has_color: bool

  @staticmethod
  def create_loglevel(level) -> LogLevel:
    if isinstance(level, LogLevel):
      return level
    if isinstance(level, str):
      return LogLevel.create(level)
    if isinstance(level, int):
      if level < len(LogLevelArgs.MAP_LEVELS):
        return LogLevel.create(LogLevelArgs.MAP_LEVELS[level])
    raise ValueError(f'invalid log level {repr(level)} of type {type(level)}')

  def __init__(self, level=DEFAULT_LEVEL, file=sys.stdout):
    self.level = Log.create_loglevel(level)
    self._file = file
    if file is None:
      self._has_color = False
    else:
      self._has_color = _has_color(file)
    self.color_enabled = self._has_color
  
  @property
  def file(self):
    return self._file
  
  @property
  def has_color(self):
    return self._has_color
  
  def enable_color(self, val=True):
    if val and self._has_color:
      self.color_enabled = val
    else:
      self.color_enabled = False
  
  def set_level(self, level):
    self.level = Log.create_loglevel(level)
  def _set_level(self, level):
    self.level = level
  
  def get_level(self) -> LogLevel:
    return self.level
  
  def _print_str(self, *args, **kwargs) -> str:
    if len(args) == 1:
      return str(args[0])
    with StringIO() as output:
      print(*args, file=output, end='', **kwargs)
      return output.getvalue()

  def _color_print(self, color, *args, end=None, flush=False, **kwargs):
    if self._has_color:
      out = self._print_str(*args, **kwargs)
      out = color + out + Color.RESET
      print(out, file=self.file, end=end, flush=flush, **kwargs)
    else:
      print(*args, file=self.file, end=end, flush=flush, **kwargs)

  def always(self, *args, file=None, **kwargs):
    print(*args, file=self.file, **kwargs)

  def error(self, *args, file=None, **kwargs):
    if self.level >= LogLevel.ERROR:
      self._color_print(Color.BRIGHT_RED, *args, **kwargs)
  
  def warn(self, *args, file=None, **kwargs):
    if self.level >= LogLevel.WARN:
      self._color_print(Color.BRIGHT_YELLOW, *args, **kwargs)

  def info(self, *args, file=None, **kwargs):
    if self.level >= LogLevel.INFO:
      print(*args, file=self.file, **kwargs)
  
  def extra(self, *args, file=None, **kwargs):
    if self.level >= LogLevel.EXTRA:
      self._color_print(Color.BRIGHT_CYAN, *args, **kwargs)

_log_loglevel = LogLevel(LogLevel.INFO)

# The default logger
_outs = Log(level=_log_loglevel, file=sys.stdout)
_outs.enable_color(True)
# Logs to `stderr`
_errs = Log(level=_log_loglevel, file=sys.stderr)
_errs.enable_color(True)

def outs() -> Log:
  return _outs

def errs() -> Log:
  return _errs

def set_log_level(level):
  level = Log.create_loglevel(level)
  _outs._set_level(level)
  _errs._set_level(level)

def enable_color(value: bool):
  _outs.enable_color(value)
  _errs.enable_color(value)
