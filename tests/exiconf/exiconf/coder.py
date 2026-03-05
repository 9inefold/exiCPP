import enum, re
import os, sys
from exiconf.logging import outs, errs
import exiconf.zbase32 as zbase32

"""
Mangling format:
  C?            : HasCookie
  0?\d*         : Version
  O             : HasOptions
  [iypc]        : Alignment
  S?            : Strict
  C?            : SelfContained
  (P[cdlip]+)?  : Preserve [Comments, Dtds, Lexicalvalues, pIs, Prefixes]
  (B\d+)?       : BlockSize (if compression)
  (M\d+)?       : ValueMaxLength
  (V\d+)?       : ValuePartitionCapacity
"""

_zbase32_alpha = 'ybndrfg8ejkmcpqxot1uwisza345h769'

_mangle_header_re = [
  '_?', # Optional leading symbol (used for C++ symbols)
  '(?P<HasCookie>C)?',
  '(?P<Version>0?\d*)',
  '(?:N|(?:O(?P<Options>.+)))',
]
_mangle_body_re = [
  '(?P<Alignment>[iypc])',
  '(?P<Strict>S)?',
  '(?P<SelfContained>C)?',
  '(?:P(?P<Preserve>c?d?l?i?p?))?',
  '(?:Y(?P<SchemaID>[{}]*)Y)?'.format(_zbase32_alpha),
  # TODO: Handle schema outside of regex
  '(?:B(?P<BlockSize>\d+))?',
  '(?:M(?P<ValueMaxLength>\d+))?',
  '(?:V(?P<ValuePartitionCapacity>\d+))?',
]

MANGLE_FLAGS = re.ASCII
MANGLE_HEADER_RE = re.compile(''.join(_mangle_header_re), MANGLE_FLAGS)
MANGLE_BODY_RE = re.compile(''.join(_mangle_body_re), MANGLE_FLAGS)

@enum.unique
class AlignmentType(enum.IntEnum):
  BitPacked = 0
  BytePacked = 1
  PreCompression = 2
  Compression = 3

  @classmethod
  def create(cls, value: str):
    match value:
      case 'i':
        return cls.BitPacked
      case 'y':
        return cls.BytePacked
      case 'p':
        return cls.PreCompression
      case 'c':
        return cls.Compression
      case _:
        raise ValueError(f'invalid align {repr(value)}')

@enum.unique
class PreserveType(enum.IntFlag):
  Comments      = 0b00001
  DTDs          = 0b00010
  LexicalValues = 0b00100
  PIs           = 0b01000
  Prefixes      = 0b10000

  @classmethod
  def create(cls, value: str):
    out = PreserveType(0)
    if value is None:
      return PreserveType(0)
    for v in value:
      match v:
        case 'c':
          out = out | cls.Comments
        case 'd':
          out = out | cls.DTDs
        case 'l':
          out = out | cls.LexicalValues
        case 'i':
          out = out | cls.PIs
        case 'p':
          out = out | cls.Prefixes
        case _:
          raise ValueError(f'invalid preserve {v}')
    return out
  
  def getnames(self) -> list[str]:
    out = []
    if not self:
      return out
    if self & self.Comments:
      out.append('Comments')
    if self & self.DTDs:
      out.append('DTDs')
    if self & self.LexicalValues:
      out.append('LexicalValues')
    if self & self.PIs:
      out.append('PIs')
    if self & self.Prefixes:
      out.append('Prefixes')
    return out

def parse_intopt(val: str, name=None, alt:int=None) -> int:
  if name is None:
    name = '<value>'
  if val is None:
    if alt is None:
      raise ValueError(f"{name} is None!")
    if not isinstance(alt, int):
      raise ValueError(f"{name} alternate '{alt}' is not an int!")
    return alt
  # Check that this is a valid number
  if not val.isdigit():
    raise ValueError(f"{name} '{val}' contains non-digits")
  return int(val)

def parse_int(D: dict[str, str], name: str, alt:int=None) -> int:
  return parse_intopt(D[name], name=name, alt=alt)

def decode_zbase32(src: str) -> str:
  if src is None:
    return None
  return zbase32.decode(src).decode()

def _demangle_options(o, B: dict[str, str]):
  o.Alignment = AlignmentType.create(B['Alignment'])
  o.Strict = B['Strict'] is not None
  o.SelfContained = B['SelfContained'] is not None
  o.Preserve = PreserveType.create(B['Preserve'])
  o.SchemaID = decode_zbase32(B['SchemaID'])
  if o.Alignment == AlignmentType.Compression:
    o.BlockSize = parse_int(B, 'BlockSize')
  else:
    o.BlockSize = 1000000
  o.ValueMaxLength = parse_int(B, 'ValueMaxLength', -1)
  o.ValuePartitionCapacity = parse_int(B, 'ValuePartitionCapacity', -1)

def parse_version(version: str) -> (bool, int):
  assert version is not None
  IsPreview = False
  Version = 0
  # Preview versions start with 0
  if version.startswith('0'):
    IsPreview = True
    version = version[1:]
  # Implicitly 1 if empty
  if len(version) == 0:
    return IsPreview, 1
  Version = parse_intopt(version, name='Version')
  # Check that this is a valid number
  if Version < 1:
    raise ValueError(f"Version '{Version}' is not >=1")
  return IsPreview, Version

def _demangle_header(o, H: dict[str, str], opt_handler=None):
  o.HasCookie = H['HasCookie'] is not None
  o.IsPreview, o.Version = parse_version(H['Version'])
  o.HasOptions = H['Options'] is not None
  if o.HasOptions and opt_handler is not None:
    o.Options = opt_handler(H['Options'])

################################################################################

class ExiOptions:
  def __init__(self, mangled=None):
    self.Alignment = None
    self.Strict = None
    self.SelfContained = None
    self.Preserve = None
    self.SchemaID = None
    self.BlockSize = None
    self.ValueMaxLength = None
    self.ValuePartitionCapacity = None
    if mangled is not None:
      self.demangle(mangled)
  
  def demangle(self, mangled: str):
    matches = MANGLE_BODY_RE.fullmatch(mangled)
    if matches is None:
      raise ValueError(f"'{mangled}' has invalid mangled options!")
    B = matches.groupdict()
    _demangle_options(self, B)

  def getdict(self):
    return {
      'Alignment': self.Alignment,
      'Strict': self.Strict,
      'SelfContained': self.SelfContained,
      'Preserve': self.Preserve,
      'SchemaID': self.SchemaID,
      'BlockSize': self.BlockSize,
      'ValueMaxLength': self.ValueMaxLength,
      'ValuePartitionCapacity': self.ValuePartitionCapacity,
    }
  def __str__(self):
    return str(self.getdict())
  def __repr__(self):
    return repr(self.getdict())
  pass

class ExiHeader:
  def __init__(self, mangled=None):
    self.HasCookie = None
    self.IsPreview = None
    self.Version = None
    self.HasOptions = None
    self.Options = None
    if mangled is not None:
      self.demangle(mangled)
  
  def demangle(self, mangled: str):
    matches = MANGLE_HEADER_RE.fullmatch(mangled)
    if matches is None:
      raise ValueError(f"'{mangled}' has an invalid mangled header!")
    H = matches.groupdict()
    _demangle_header(o, H, opt_handler=demangle_options)
    
  def getdict(self):
    return {
      'HasCookie': self.HasCookie,
      'IsPreview': self.IsPreview,
      'Version': self.Version,
      'HasOptions': self.HasOptions,
      'Options': self.Options,
    }
  def __str__(self):
    return str(self.getdict())
  def __repr__(self):
    return repr(self.getdict())
  pass

def demangle_options(mangled: str) -> ExiOptions:
  matches = MANGLE_BODY_RE.fullmatch(mangled)
  if matches is None:
    errs.error(f"'{mangled}' has invalid mangled options!")
    return None
  B = matches.groupdict()
  o = ExiOptions()
  # Parse out
  _demangle_options(o, B)
  return o

def demangle_header(mangled: str) -> ExiHeader:
  matches = MANGLE_HEADER_RE.fullmatch(mangled)
  if matches is None:
    errs.error(f"'{mangled}' has an invalid mangled header!")
    return None
  H = matches.groupdict()
  o = ExiHeader()
  # Parse out
  _demangle_header(o, H, opt_handler=demangle_options)
  return o

################################################################################

import json

class ExiHeaderJSONEncoder(json.JSONEncoder):
  def default(self, obj):
    if isinstance(obj, ExiHeader):
      return obj.getdict()
    if isinstance(obj, ExiOptions):
      d = obj.getdict()
      Alignment = d['Alignment']
      if Alignment is not None:
        d['Alignment'] = Alignment.name
      Preserve = d['Preserve']
      if Preserve is not None:
        d['Preserve'] = Preserve.getnames()
      return d
    if isinstance(obj, (enum.IntEnum, enum.IntFlag)):
      return repr(obj)
    # Let the base class default method raise the TypeError
    return super().default(obj)

def _try_demangle(mangled: str):
  try:
    v = demangle_header(mangled)
    if v is None:
      return
    r = json.dumps(v, indent=2, cls=ExiHeaderJSONEncoder)
    print(f"{mangled}: {r}")
  except ValueError as e:
    print(f'{mangled}: {e}')
