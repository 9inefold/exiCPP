from exiconf.logging import Log, Color, ColorType

__all__ = ['TestCounter']

def _get_ratio_color(ratio: float) -> ColorType:
  # Calc ratio
  if ratio >= 92.0:
    return Color.BRIGHT_GREEN
  elif ratio >= 87.0:
    return Color.GREEN
  elif ratio >= 79.0:
    return Color.YELLOW
  elif ratio >= 70.0:
    return Color.BRIGHT_YELLOW
  else:
    return Color.BRIGHT_RED

class TestCounter:
  __slots__ = ('_passed', '_failed', '_skipped',)
  _passed: int
  _failed: int
  _skipped: int

  def __init__(self):
    self._passed = 0
    self._failed = 0
    self._skipped = 0
  
  @property
  def passed(self) -> int:
    return self._passed
  @property
  def failed(self) -> int:
    return self._failed
  @property
  def skipped(self) -> int:
    return self._skipped
  
  def add_passed(self):
    self._passed += 1
  def add_failed(self):
    self._failed += 1
  def add_skipped(self):
    self._skipped += 1
  
  @property
  def total_passed(self) -> int:
    return self._passed + self._skipped
  @property
  def total(self) -> int:
    return self.total_passed + self._failed
  
  def percent_passed(self) -> float:
    if self._failed == 0:
      return 100.0
    return (self.total_passed / self.total) * 100.0
  
  def percent_and_color(self) -> tuple[float, ColorType]:
    ratio = self.percent_passed()
    return ratio, _get_ratio_color(ratio)
