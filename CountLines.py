import argparse
from glob import glob
import re
import mmap
from os.path import join, getsize, normpath

parser = argparse.ArgumentParser(
                  prog='CountLines',
                  description='Counts lines in the current project')
parser.add_argument('--noempty', action='store_true',
                    help='If empty lines should be ignored')
parser.add_argument('--nocomments', action='store_true',
                    help='If comments should be ignored')
parser.add_argument('--header', action='store_true',
                    help='If header should be counted')

args = parser.parse_args()
SKIP_EMPTY = args.noempty
SKIP_COMMENTS = args.nocomments
SKIP_HEADER = not args.header

IS_CPP_FILE = re.compile(r'.*\.(hpp|cpp|impl|mac|in)')
CPP_COMMENT = '//'.encode()
CMAKE_COMMENT = '#'.encode()

# From https://stackoverflow.com/a/850962/17980859
def mapcount(filename: str):
  if getsize(filename) == 0:
    return 0

  not_txt = not filename.endswith('txt')
  strip_lines = not_txt and (SKIP_EMPTY or SKIP_COMMENTS)
  skip_comments = not_txt and SKIP_COMMENTS
  comment_str = CMAKE_COMMENT if filename.endswith('cmake') else CPP_COMMENT

  with open(filename, "r+") as f:
    buf = mmap.mmap(f.fileno(), 0)
    lines = 0
    readline = buf.readline
    # Skip the header
    if SKIP_HEADER and IS_CPP_FILE.match(filename):
      while True:
        line = readline()
        if not line:
          return lines
        if not line.startswith(CPP_COMMENT):
          break
    # Do the actual counting
    while True:
      line = readline()
      if not line:
        break
      if strip_lines:
        line = line.lstrip()
        if SKIP_EMPTY and (len(line) == 0):
          continue
        if skip_comments and line.startswith(comment_str):
          continue
      lines += 1
    return lines

def glob_files_in(pattern, lib):
  r = []
  r.extend(glob(pattern, root_dir=f"./{lib}", recursive=True))
  return [join(lib, f) for f in r]

def glob_files(extension: str) -> [str]:
  f = []
  f.extend(glob(f'./*.{extension}'))
  recurse = f'**/*.{extension}'
  f.extend(glob_files_in(recurse, 'include'))
  f.extend(glob_files_in(recurse, 'lib'))
  f.extend(glob_files_in(recurse, 'redirect'))
  return f

def get_files() -> [str]:
  f = []
  f.extend(glob_files('txt'))
  f.extend(glob_files('cmake'))
  f.extend(glob_files('cpp'))
  f.extend(glob_files('hpp'))
  f.extend(glob_files('mac'))
  f.extend(glob_files('impl'))
  f.extend(glob_files('in'))

  p = re.compile(r'(.+[\/])*[^\/]+\.hidden[^\/]+')
  f = [normpath(s) for s in f]
  return [s for s in f if not p.match(s)]

count = 0
for f in get_files():
  lines = mapcount(f)
  count += lines
print(f'Line Count: {count}\n')
