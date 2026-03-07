import os, time, platform, ctypes
from sys import stdout, stderr

__all__ = ['has_color']

def _check_term_ansi() -> bool:
  return 'TERM' in os.environ and os.environ['TERM'] == 'ANSI'

def _check_isatty(handle) -> bool:
  return hasattr(handle, "isatty") and handle.isatty()

_is_windows = (platform.system() == 'Windows')
_is_ansi_term = _check_term_ansi()

_kernel32 = None
_msvcrt = None

def _init_windows():
  global _kernel32, _msvcrt
  import msvcrt
  _kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
  _msvcrt = msvcrt

def _check_when_windows(handle) -> bool:
  global _kernel32, _msvcrt
  if _kernel32 is None:
    _init_windows()
  from ctypes.wintypes import DWORD
  h = _msvcrt.get_osfhandle(handle.fileno())
  out = DWORD()
  res = _kernel32.GetConsoleMode(h, ctypes.byref(out))
  if not res:
    return False
  # Check ENABLE_VIRTUAL_TERMINAL_PROCESSING
  return (out.value & 0x004) != 0

def _check_handle(handle) -> bool:
  try:
    if _is_ansi_term or _check_isatty(handle):
      if platform.system() == 'Windows' and not _is_ansi_term:
        return _check_when_windows(handle)
      else:
        return True
    else:
      return False
  except:
    return False

_color_check = {
  k: _check_handle(k) for k in [stdout, stderr]
}

def force_color(handle, state: bool):
  global _color_check
  assert isinstance(state, bool)
  _color_check[handle] = state

def has_color(handle) -> bool:
  global _color_check
  if handle not in _color_check:
    res = _check_handle(handle)
    _color_check[handle] = res
    return res
  return bool(_color_check[handle])
