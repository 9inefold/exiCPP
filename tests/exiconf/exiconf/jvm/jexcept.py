import re, traceback
from jpype.types import JException
from exiconf.logging import Log
from exiconf.jvm.check import jvm_check
from exiconf.jvm.mapping import lookup_line_ext

__all__ = [
  'format_jexception',
  'print_jexception',
  'log_jexception'
]

_jvm_check = jvm_check(__file__)

# Handles formatting jexceptions on the java side. Formatted like normal python exceptions
def _format_jexception_like_py(ex: JException) -> list[str]:
  found_nonerr = False
  frames = ex.getStackTrace()
  out = []
  for frame in frames:
    _method = str(frame.getMethodName())
    _clazz = str(frame.getClassName())
    # Skip reportError and friends
    if not found_nonerr:
      if _clazz.startswith('org.apache.xerces.'):
        if re.fullmatch("report([A-Z][a-z]+)?Error", _method):
          continue
      found_nonerr = True

    to_push = '  File '
    _file = frame.getFileName()
    _line = frame.getLineNumber()
    _line_data = ''
    if _file:
      to_push += f'"{_file}"'
    else:
      to_push += "<unknown>"
    if _line > 0:
      to_push += f', line {_line}'
      try:
        _line_data = lookup_line_ext(_clazz, _file, _line)
      except Exception as e:
        #print("{{")
        #traceback.print_tb(e.__traceback__)
        #print(e)
        #print("}}", flush=True)
        pass
    to_push += f', in {_clazz}.{_method}\n'
    if len(_line_data) > 0:
      to_push += f'    {_line_data}\n'
    out.append(to_push)
  return list(reversed(out))

# Formats jexception trace to a string
def format_jexception(ex: JException, skip=0) -> list[str]:
  _jvm_check()
  stacks = traceback.format_stack()[:-(skip + 2)]
  stacks.extend(traceback.format_tb(ex.__traceback__))
  stacks.extend(_format_jexception_like_py(ex))
  return stacks

# Prints jexception trace
def print_jexception(ex: JException, skip=0):
  _jvm_check()
  stacks = format_jexception(ex, skip=(skip + 1))
  print("Traceback (most recent call last):\n",
        ''.join(stacks), f'{ex.toString()}\n', sep=''
  )

# Writes jexception trace to a Log object
def log_jexception(ex: JException, logger: Log, skip=0):
  _jvm_check()
  stacks = format_jexception(ex, skip=(skip + 1))
  logger.error("Traceback (most recent call last):\n",
               ''.join(stacks), f'{ex.toString()}\n', sep=''
  )
