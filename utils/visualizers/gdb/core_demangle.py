import re
import gdb # pyright: ignore[reportMissingModuleSource]

def make_printer(cls, re_string):
  this_re = re.compile(re_string)
  def lookup_impl(val):
    lookup_tag = val.type.strip_typedefs().unqualified().tag
    if lookup_tag is None:
      return None
    if this_re.match(lookup_tag):
      return cls(val)
    return None
  # Now add the lookup function
  gdb.pretty_printers.append(lookup_impl)

# Encapsulates a list of children. Calls .child() on the pretty printer only
# when requested. This is a workaround for poor performance when inspecting
# large arrays in the debugger.
# From https://github.com/decodeless/offset_ptr/blob/main/debugging/offset_span_pretty_printer.py
class LazyChildren:
  __slots__ = ('_obj', '_length',)

  def __init__(self, obj, length):
    self._obj = obj
    self._length = length

  def __getitem__(self, index):
    if not 0 <= index < self._length:
      raise IndexError('Index out of range')
    return self._obj.child(index)

  def __len__(self):
    return self._length

class PODSmallVectorPrinter:
  """Print an exi::itanium_demangle::PODSmallVector"""
  __slots__ = ('__T', '__beg', '__end',)
  
  def __init__(self, val: gdb.Value):
    raw_type = val.type.strip_typedefs().unqualified()
    self.__T = raw_type.template_argument(0)
    self.__beg = val['First']
    self.__end = val['Last']

  @property
  def length(self):
    return self.__end - self.__beg

  def child(self, i):
    return (f'[{i}]', (self.__beg + i).dereference())

  def children(self):
    return LazyChildren(self, self.length)

  def __len__(self):
    return self.length

  def to_string(self):
    return f'PODSmallVector<{self.__T}> of length {self.length}'

  def display_hint(self):
    return 'array'

make_printer(PODSmallVectorPrinter, "^exi::itanium_demangle::PODSmallVector<.*>$")

