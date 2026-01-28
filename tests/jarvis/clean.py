import sys, os
from pathlib import Path

#print(sys.argv)

dir_path = Path(__file__).parent
args = sys.argv[1:]
outls = []

for arg in args:
  files = arg.split(';')
  for f in files:
    f = Path(f)
    if f.exists() and f.is_file():
      os.remove(str(f))
      try:
        outls.append(f.relative_to(dir_path).as_posix())
      except:
        outls.append(f.as_posix())

print('cleaned:', ', '.join(outls))
