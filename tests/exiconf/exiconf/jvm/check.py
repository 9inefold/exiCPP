import sys, traceback
from exiconf.jvm.setup import get_jvm_path

__all__ = ['jvm_check', 'do_jvm_check']

# Allows checks to be skipped
_internal_check = False

# Handles failing check
def _do_check_fail(filename=None):
  out = ''
  if filename is not None:
    out += f'In {filename}:\n'
  out += 'Expected JVM to be started!\n'
  out += ''.join(traceback.format_stack()[:-1])
  print(out, flush=True)
  sys.exit(1)

# Handles passing check
def _do_check_pass(filename=None):
  pass

# Returns a functor which can be used to verify the jvm has been initialized
def jvm_check(filename=None):
  def check_impl():
    global _internal_check
    if _internal_check:
      return
    if get_jvm_path() is None:
      _do_check_fail(filename)
    _internal_check = True
  return check_impl

# Verifies the jvm has been initialized. If not, exits with an error
def do_jvm_check(filename=None):
  global _internal_check
  if _internal_check:
    return
  if get_jvm_path() is None:
    _do_check_fail(filename)
  _internal_check = True
