from packaging.version import parse as _parse_version, Version as VersionTuple

__all__ = ['version_tuple']

def _handle_multi_args(args: list[any], _recurse):
  joined = ".".join(str(v) for v in args)
  return _parse_version(joined)

def _handle_single_arg(arg, _recurse):
  if type(arg) == str:
    return _parse_version(arg)
  elif type(arg) == list or type(arg) == tuple:
    return version_tuple(*arg, _recurse=_recurse+1)
  pass

# Creates a VersionTuple from args. Input can be a string, tuple, or list
def version_tuple(*args, _recurse=0) -> VersionTuple:
  if _recurse > 2:
    raise RecursionError('version_tuple exceeded recursion limit of 2')
  if len(args) == 0:
    return VersionTuple('0')
  elif len(args) == 1:
    return _handle_single_arg(args[0], _recurse)
  else:
    return _handle_multi_args([*args], _recurse)
