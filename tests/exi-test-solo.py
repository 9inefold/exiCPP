import os, sys, argparse
from pathlib import Path
from glob import glob

cwd = os.getcwd()
EXI_CURR_DIR = Path(__file__).parent
sys.path.insert(0, str(EXI_CURR_DIR / 'exiconf'))

from exiconf.jvm.setup import start_jvm # type: ignore
from exiconf.logging import LogLevel, outs, errs, set_log_level
from exiconf.constants import TEST_SRC_DIR, EXICPP_EXECUTABLE
from exiconf.diff import diff_xml
from exiconf.exicpp_coder import ExicppCoder

CODER_NAMES = [
  # Exicpp, OpenEXI, Exificient encode
  'i', 'o', #'x',
]

def get_coder(typ: str, mangled: str):
  from exiconf.openexi_coder import OpenEXICoder
  match typ:
    case 'i':
      return ExicppCoder(mangled, outs())
    case 'o':
      return OpenEXICoder(mangled, outs())
    case 'x':
      raise NotImplementedError('exificient coder is not implemented yet!')
    case _:
      raise ValueError(f"expected {CODER_NAMES}, got '{typ}'")

class SoloArgNamespace(argparse.Namespace):
  root: Path
  file: str
  mangling: str
  coder: str

def get_arg_parser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(prog="exi-test-solo")
  parser.add_argument(
    '--root',
    action='store',
    help='use a different root folder',
    metavar='FOLDER',
    type=Path,
    default=TEST_SRC_DIR
  )
  parser.add_argument(
    'file',
    help='the name of the file in test form (eg. xml/097.xml is xml.079)',
    type=_convert_file,
  )
  parser.add_argument(
    'mangling',
    help='the mangling used for the encoding/decoding',
    type=str,
  )
  parser.add_argument(
    'coder',
    help='the id of the coder to use',
    choices=CODER_NAMES,
    type=str,
  )
  return parser

def _convert_file(file) -> str:
  return str(file).replace('.', '/') + '.xml'

def parse_args() -> SoloArgNamespace:
  args = sys.argv[1:]
  parser = get_arg_parser()
  ns = SoloArgNamespace()
  return parser.parse_args(args, namespace=ns)

if __name__ == '__main__':
  set_log_level(LogLevel.EXTRA)
  args = parse_args()
  if args.coder == 'i':
    raise NotImplementedError('Exicpp coders are not supported')
  # Make sure the file is real
  f = args.root / args.file
  if not f.exists():
    raise FileNotFoundError(f.as_posix(), 'does not exist')
  # Handle coding
  start_jvm(jvm_args=['-ea', '-Dexicpp.loglevel=verbose'])
  coder = get_coder(args.coder, args.mangling)

  xml_out = None
  try:
    xml_in = f.read_text('utf8')
    data = coder.encode(xml_in, f)
    if data is None:
      outs().extra(f'encoding {f.as_posix()} failed', flush=True)
      sys.exit(1)
    xml_out = coder.decode(data, f)
    if xml_out is None:
      outs().extra(f'decoding {f.as_posix()} failed', flush=True)
      sys.exit(1)
    # Print results
    diff = diff_xml(f, xml_in, xml_out)
    if diff:
      outs().extra(f'{f.as_posix()}: DIFF', flush=True)
    else:
      outs().extra(f'{f.as_posix()}: SAME', flush=True)
  except Exception as e:
    outs().extra(f'{f.as_posix()}*: {type(e).__name__}: {e}', flush=True)
    sys.exit(1)

  