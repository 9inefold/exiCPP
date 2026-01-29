import argparse
from os import remove
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('-q', '--quiet', action='store_true', help='silence output')
parser.add_argument('--files', action='extend', nargs='*', type=str, default=[], help='files to clean')
clargs = parser.parse_args()

dir_path = Path(__file__).parent
args = clargs.files
outls = []

for arg in args:
  files = arg.split(';')
  for f in files:
    f = Path(f)
    if f.exists() and f.is_file():
      remove(str(f))
      try:
        outls.append(f.relative_to(dir_path).as_posix())
      except:
        outls.append(f.as_posix())

if not clargs.quiet:
  print('cleaned:', ', '.join(outls))
