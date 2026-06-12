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
  set_log_level(args.diag_level)
  enable_color(args.color)

  # Needs to be started before adding the test runner!
  start_jvm(
    jvm_path=args.jvm_path,
    classpath=args.jvm_classpath,
    jvm_args=args.jvm_args
  )

  from exiconf.test.runner import runner_main
  runner_main(args, extra_args)
