import sys, traceback
from exiconf.jvm.setup import get_jvm_path

__all__ = ['jvm_check', 'do_jvm_check']

_internal_check = False

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

def jvm_check(filename=None):
  def check_impl():
    global _internal_check
    if _internal_check:
      return
    if get_jvm_path() is None:
      _do_check_fail(filename)
    _internal_check = True
  return check_impl

def do_jvm_check(filename=None):
  global _internal_check
  if _internal_check:
    return
  if get_jvm_path() is None:
    _do_check_fail(filename)
  _internal_check = True
