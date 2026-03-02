import enum
import os, sys

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

# A class used for logging
class Log:
  DEFAULT_LEVEL = LogLevel.ERROR

  @staticmethod
  def create_loglevel(level) -> LogLevel:
    if isinstance(level, LogLevel):
      return level
    if isinstance(level, str):
      return LogLevel.create(level)
    if isinstance(level, int):
      if level < len(LogLevelArgs.MAP_LEVELS):
        return LogLevel.create(LogLevelArgs.MAP_LEVELS[level])
    raise ValueError(f'invalid log level {repr(value)} of type {type(value)}')

  def __init__(self, level=DEFAULT_LEVEL, file=sys.stdout):
    self.level = Log.create_loglevel(level)
    self.file = file
  
  def set_level(self, level):
    self.level = Log.create_loglevel(level)
  
  def error(self, *args, file=None, **kwargs):
    if self.level >= LogLevel.ERROR:
      print(*args, file=self.file, **kwargs)
  
  def warn(self, *args, file=None, **kwargs):
    if self.level >= LogLevel.WARN:
      print(*args, file=self.file, **kwargs)

  def info(self, *args, file=None, **kwargs):
    if self.level >= LogLevel.INFO:
      print(*args, file=self.file, **kwargs)
  
  def extra(self, *args, file=None, **kwargs):
    if self.level >= LogLevel.EXTRA:
      print(*args, file=self.file, **kwargs)

_log_loglevel = LogLevel(LogLevel.INFO)

# The default logger
outs = Log(level=_log_loglevel)
# Logs to `stderr`
errs = Log(level=_log_loglevel, file=sys.stderr)

def set_log_level(level):
  level = Log.create_loglevel(level)
  outs.set_level(level)
  errs.set_level(level)
