from exiconf.logging import Log, LogLevel
from exiconf.cl_args import parse_args
from exiconf.logging import set_log_level, enable_color
from exiconf.jvm.setup import start_jvm

__all__ = ['main']

FOLDER_NAMES = [
  # Exicpp, OpenEXI, Exificient encode
  'i', 'o', #'x',
  # Exicpp roundtrip and decode
  'ii', 'io', #'ix',
  # OpenEXI roundtrip and decode
  'oo', 'oi', #'ox',
  # Exificient roundtrip and decode
  #'xx', 'xo', 'xi',
]

# The default program entry point
def main(extra_args={}):
  args = parse_args()
  diag_level = Log.create_loglevel(args.diag_level)
  set_log_level(diag_level)
  enable_color(args.color)

  has_info_level = (diag_level >= LogLevel.INFO)
  if args.print_passed is None:
    args.print_passed = has_info_level
  if args.print_skipped is None:
    args.print_skipped = has_info_level

  # Needs to be started before adding the test runner!
  start_jvm(
    jvm_path=args.jvm_path,
    classpath=args.jvm_classpath,
    jvm_args=args.jvm_args
  )

  from exiconf.test.runner import runner_main
  runner_main(args, extra_args)
