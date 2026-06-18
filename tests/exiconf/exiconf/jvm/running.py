from jpype._core import JVMNotRunning, JVMNotFoundException

__all__ = ['is_fatal_exception']

def is_fatal_exception(e: Exception) -> bool:
  return isinstance(e, JVMNotRunning)
