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

"""
parse_options boolean arguments:

  attribute_defaults - inject default attributes from DTD or XMLSchema
  dtd_validation - validate against a DTD referenced by the document
  load_dtd - use DTD for parsing
  no_network - prevent network access for related files (default: True)
  ns_clean - clean up redundant namespace declarations
  recover - try hard to parse through broken XML
  remove_blank_text - discard blank text nodes that appear ignorable
  remove_comments - discard comments
  remove_pis - discard processing instructions
  strip_cdata - replace CDATA sections by normal text content (default: True)
  compact - save memory for short text content (default: True)
  collect_ids - use a hash table of XML IDs for fast access (default: True, always True with DTD validation)
  resolve_entities - replace entities by their text value (default: True)

  huge_tree - disable security restrictions and support very deep trees
      and very long text content (only affects libxml2 2.7+)

Other parse_options arguments:

  encoding - override the document encoding
  target - a parser target object that will receive the parse events
  schema - an XMLSchema to validate against
"""

"""
diff_options arguments:

  F - similarity (default: 0.5)
  uniqueattrs - list of attributes or (tag, attribute) pairs that uniquely identifies a node inside a document. (default: 'xml:id')
  ratio_mode - 'fast', 'faster', or 'accurate' (default: 'fast')
  fast_match - (default: False)
  best_match - (default: False)
  ignored_attrs - (default: [])
"""

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
  try:
    diff_list = diff_func(arg1, arg2, **kwargs)
    return _diff_common(name, diff_list, _or(logger, errs()))
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
