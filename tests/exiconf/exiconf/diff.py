import sys
from exiconf.constants import EXI_BASE_DIR
from exiconf.logging import errs
sys.path.insert(0, str(EXI_BASE_DIR / 'vendored' / 'xmldiff'))
from xmldiff import main
from xmldiff.actions import UpdateTextIn, UpdateTextAfter
from lxml import etree

__all__ = [
  'diff_xml',
  'diff_trees',
  'diff_files',
]

def _or(value, other):
  return value if value is not None else other

# Compares the diff list
def _diff_common(name, diff_list, logger) -> bool:
  real_diffs = []
  # Fixup diffs with empty data
  # TODO: Add option to compare without preserves
  for diff in diff_list:
    if isinstance(diff, (UpdateTextIn, UpdateTextAfter)):
      old = _or(diff.oldtext, '')
      new = _or(diff.text, '')
      if old.strip() != new.strip():
        real_diffs.append(diff)
    else:
      real_diffs.append(diff)
  if len(real_diffs) != 0:
    print(f'[diff] {Path(name).as_posix()}: {real_diffs}')
    return False
  return True

def _diff_xml(diff_func, name, arg1, arg2, logger, **kwargs) -> bool:
  if logger is None:
    logger = errs()
  try:
    diff_list = diff_func(arg1, arg2, **kwargs)
    return _diff_common(name, diff_list, logger)
  except Exception as e:
    logger.error(f'[diff] {Path(name).as_posix()}*: {type(e).__name__}: {e}')
    return False

# Returns true if xml is not different.
def diff_xml(name, xml1, xml2, logger=None, **kwargs) -> bool:
  bytes1 = str(xml1).encode()
  bytes2 = str(xml2).encode()
  return _diff_xml(main.diff_texts, name, bytes1, bytes2, logger, **kwargs)

# Returns true if xml is not different.
def diff_trees(name, node1, node2, logger=None, **kwargs) -> bool:
  return _diff_xml(main.diff_trees, name, node1, node2, logger, **kwargs)

# Returns true if xml is not different.
def diff_files(name, file1, file2, logger=None, **kwargs) -> bool:
  return _diff_xml(main.diff_files, name, file1, file2, logger, **kwargs)
