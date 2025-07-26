from glob import glob
import re
import mmap
from os.path import join, getsize, normpath

# From https://stackoverflow.com/a/850962/17980859
def mapcount(filename):
  if getsize(filename) == 0:
    return 0
  with open(filename, "r+") as f:
    buf = mmap.mmap(f.fileno(), 0)
    lines = 0
    readline = buf.readline
    while readline():
      lines += 1
    return lines

def glob_files_in(pattern, lib):
  r = []
  r.extend(glob(pattern, root_dir=f"./{lib}", recursive=True))
  return [join(lib, f) for f in r]

def glob_files(extension):
  f = []
  f.extend(glob(f'./*.{extension}'))
  recurse = f'**/*.{extension}'
  f.extend(glob_files_in(recurse, 'include'))
  f.extend(glob_files_in(recurse, 'lib'))
  f.extend(glob_files_in(recurse, 'redirect'))
  return f

def get_files():
  f = []
  f.extend(glob_files('txt'))
  f.extend(glob_files('cmake'))
  f.extend(glob_files('cpp'))
  f.extend(glob_files('hpp'))
  f.extend(glob_files('mac'))
  f.extend(glob_files('impl'))

  p = re.compile('.*/[^/]*.hidden[^/]*')
  f = [normpath(s).replace('\\', '/') for s in f]
  return [normpath(s) for s in f if not p.match(s)]

count = 0
for f in get_files():
  lines = mapcount(f)
  count += lines
print(f'Line Count: {count}\n')
