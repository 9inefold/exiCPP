import os, sys, functools
from typing import TextIO, Any

if os.name == 'nt':
  try:
    import msvcrt, ctypes, ctypes.wintypes
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    _has_windows_imports = True
  except (ModuleNotFoundError, ImportError):
    _has_windows_imports = False

__all__ = ['has_color']

def _check_isatty(handle) -> bool:
  return hasattr(handle, "isatty") and handle.isatty()

def _check_is_ansi_console(handle) -> bool:
  if _check_isatty(handle):
    return True
  return os.environ.get('TERM') == 'ANSI'

if _has_windows_imports:
  def _enable_console_color(file_out: TextIO | Any) -> bool:
    handle = msvcrt.get_osfhandle(file_out.fileno())
    curr_mode = ctypes.wintypes.DWORD()
    if kernel32.GetConsoleMode(handle, ctypes.byref(curr_mode)):
      # ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x004
      if (curr_mode.value & 0x004) != 0:
        return True
      # Try enabling ANSI mode
      mode = curr_mode.value | 0x004
      if kernel32.SetConsoleMode(handle, mode):
        return True
    return False

# Things consistent across handles
def _default_terminal_setup() -> bool:
  # Windows handling
  if os.name == 'nt':
    if 'ANSICON' in os.environ:
      return True
    elif 'WT_SESSION' in os.environ:  # Windows Terminal
      return True
    elif os.environ.get('TERM_PROGRAM') == 'vscode':
      return False
  else: # Unix-like systems
    term = os.environ.get('TERM', '')
    if term in ['dumb', '']:
      return False
  return True

# Save this to avoid work
DEFAULT_CHECKS = _default_terminal_setup()

def handle_supports_color(handle: TextIO | Any) -> bool:
  # Check this is actually a console
  if not _check_is_ansi_console(handle):
    return False
  elif not DEFAULT_CHECKS:
    return False

  # Windows handling
  if os.name == 'nt' and _has_windows_imports:
    try:
      return _enable_console_color(handle)
    except Exception:
      pass
    # Not enabled
    return False
  else:
    # Other systems should be ok
    return True

@functools.cache
def has_color(handle) -> bool:
  return handle_supports_color(handle)
