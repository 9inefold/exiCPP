import sys, traceback
from exiconf.jvm.setup import _used_jvm_path

def _do_check_fail(filename=None):
  out = ''
  if filename is not None:
    out += f'In {filename}:\n'
  out += 'Expected JVM to be started!\n'
  out += ''.join(traceback.format_stack()[:-1])
  print(out, flush=True)
  sys.exit(1)

def _do_check_pass(filename=None):
  pass

do_check = _do_check_fail if (_used_jvm_path is None) else _do_check_pass
