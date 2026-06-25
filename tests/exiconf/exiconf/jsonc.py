import json, re
from json import dump, dumps
from json import detect_encoding
from pathlib import Path

__all__ = ['dump', 'dumps', 'load', 'loads']

_NOT_NEWLINE = re.compile(r"[^\r\n]+")
_SINGLE_LINE_COMMENT = re.compile(r"\/\/.*\r?\n")
_MULTI_LINE_COMMENT = re.compile(r"\/\*(.*?)\*\/", re.DOTALL)

def _multi_line_sub(m: re.Match[str]) -> str:
  return _NOT_NEWLINE.sub(' ', m.group(1))

def _strip_comments(s: str) -> str:
  s = _SINGLE_LINE_COMMENT.sub('\n', s)
  return _MULTI_LINE_COMMENT.sub(_multi_line_sub, s)

_STRING_LITERAL = re.compile(r"\"(?:[^\"\\]|\\.)*\"")
_TRAILING_COMMA = re.compile(r",(\s*[\}\]])", re.DOTALL) 

def _remove_trailing_comma(s: str) -> str:
  return _TRAILING_COMMA.sub(r'\1', s)

def _jsonc_to_json(s: str) -> str:
  parts: list[str] = []
  last_end = 0
  s = _strip_comments(s)
  for m in _STRING_LITERAL.finditer(s):
    # Get everything before the string literal
    outside = s[last_end:m.start()]
    parts.append(_remove_trailing_comma(outside))
    # Add the string literal
    parts.append(m.group())
    last_end = m.end()
  # Remainder after the last string literal
  parts.append(_remove_trailing_comma(s[last_end:]))
  return ''.join(parts)

def load(fp, *, cls=None, object_hook=None, parse_float=None,
         parse_int=None, parse_constant=None, object_pairs_hook=None, **kw):
  """Deserialize ``fp`` (a ``.read()``-supporting file-like object containing
  a JSON document) to a Python object.

  ``object_hook`` is an optional function that will be called with the
  result of any object literal decode (a ``dict``). The return value of
  ``object_hook`` will be used instead of the ``dict``. This feature
  can be used to implement custom decoders (e.g. JSON-RPC class hinting).

  ``object_pairs_hook`` is an optional function that will be called with the
  result of any object literal decoded with an ordered list of pairs.  The
  return value of ``object_pairs_hook`` will be used instead of the ``dict``.
  This feature can be used to implement custom decoders.  If ``object_hook``
  is also defined, the ``object_pairs_hook`` takes priority.

  To use a custom ``JSONDecoder`` subclass, specify it with the ``cls``
  kwarg; otherwise ``JSONDecoder`` is used.
  """
  return loads(fp.read(),
    cls=cls, object_hook=object_hook,
    parse_float=parse_float, parse_int=parse_int,
    parse_constant=parse_constant, object_pairs_hook=object_pairs_hook, **kw)

_default_decoder = json.JSONDecoder(object_hook=None, object_pairs_hook=None)

def loads(s, *, cls=None, object_hook=None, parse_float=None,
          parse_int=None, parse_constant=None, object_pairs_hook=None, **kw):
  """Deserialize ``s`` (a ``str``, ``bytes`` or ``bytearray`` instance
  containing a JSON document) to a Python object.

  ``object_hook`` is an optional function that will be called with the
  result of any object literal decode (a ``dict``). The return value of
  ``object_hook`` will be used instead of the ``dict``. This feature
  can be used to implement custom decoders (e.g. JSON-RPC class hinting).

  ``object_pairs_hook`` is an optional function that will be called with the
  result of any object literal decoded with an ordered list of pairs.  The
  return value of ``object_pairs_hook`` will be used instead of the ``dict``.
  This feature can be used to implement custom decoders.  If ``object_hook``
  is also defined, the ``object_pairs_hook`` takes priority.

  ``parse_float``, if specified, will be called with the string
  of every JSON float to be decoded. By default this is equivalent to
  float(num_str). This can be used to use another datatype or parser
  for JSON floats (e.g. decimal.Decimal).

  ``parse_int``, if specified, will be called with the string
  of every JSON int to be decoded. By default this is equivalent to
  int(num_str). This can be used to use another datatype or parser
  for JSON integers (e.g. float).

  ``parse_constant``, if specified, will be called with one of the
  following strings: -Infinity, Infinity, NaN.
  This can be used to raise an exception if invalid JSON numbers
  are encountered.

  To use a custom ``JSONDecoder`` subclass, specify it with the ``cls``
  kwarg; otherwise ``JSONDecoder`` is used.
  """
  if isinstance(s, str):
    if s.startswith('\ufeff'):
      raise json.JSONDecodeError("Unexpected UTF-8 BOM (decode using utf-8-sig)",
                                 s, 0)
  else:
    if not isinstance(s, (bytes, bytearray)):
      raise TypeError(f'the JSON object must be str, bytes or bytearray, '
                      f'not {s.__class__.__name__}')
    s = s.decode(detect_encoding(s), 'surrogatepass')

  if (cls is None and object_hook is None and
    parse_int is None and parse_float is None and
    parse_constant is None and object_pairs_hook is None and not kw):
    return _default_decoder.decode(s)
  if cls is None:
    cls = json.JSONDecoder
  if object_hook is not None:
    kw['object_hook'] = object_hook
  if object_pairs_hook is not None:
    kw['object_pairs_hook'] = object_pairs_hook
  if parse_float is not None:
    kw['parse_float'] = parse_float
  if parse_int is not None:
    kw['parse_int'] = parse_int
  if parse_constant is not None:
    kw['parse_constant'] = parse_constant
  return cls(**kw).decode(_jsonc_to_json(s))

def detect_loads_ext(ext: str):
  if ext == '.jsonc':
    return loads
  if ext != '.json':
    raise ValueError(f"Extension '{ext}' is not a json type!")
  return json.loads

def detect_loads(path: Path | str):
  if isinstance(path, str):
    path = Path(path)
  return detect_loads_ext(path.suffix)

def loads_from_path(path: Path | str, *, cls=None, object_hook=None,
                    parse_float=None, parse_int=None, parse_constant=None,
                    object_pairs_hook=None, **kw):
  if isinstance(path, str):
    path = Path(path)
  loads_type = detect_loads_ext(path.suffix)
  data = path.read_text('utf8')
  return loads_type(data,
    cls=cls, object_hook=object_hook,
    parse_float=parse_float, parse_int=parse_int,
    parse_constant=parse_constant, object_pairs_hook=object_pairs_hook, **kw)
