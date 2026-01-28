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

IS_CPP_STYLE_FILE = re.compile(r'.*\.(hpp|cpp|impl|mac|in|java)')
CSTYLE_COMMENT = '//'.encode()
PYSTYLE_COMMENT = '#'.encode()

class LineHandler:
  def __init__(self, filename: str):
    not_txt = not filename.endswith('txt')
    cpp_style = IS_CPP_STYLE_FILE.match(filename)

    self.line_count = 0
    self.strip_lines = not_txt and (SKIP_EMPTY or SKIP_COMMENTS)
    self.skip_comments = not_txt and SKIP_COMMENTS
    self.skip_header = cpp_style and SKIP_HEADER

    if cpp_style:
      self.comment_str = CSTYLE_COMMENT
    else:
      self.comment_str = PYSTYLE_COMMENT
  
  def counts(self, line: AnyStr) -> int:
    if strip_lines:
      line = line.lstrip()
    if SKIP_EMPTY and (len(line) == 0):
      return 0
    if self.skip_comments and line.startswith(self.comment_str):
      return 0
    return 1
  
  def add(self, line: AnyStr):
    self.line_count = self.counts(line)

# From https://stackoverflow.com/a/850962/17980859
def mapcount(filename: str):
  if getsize(filename) == 0:
    return 0

  handler = LineHandler(filename)
  with open(filename, "r+") as f:
    buf = mmap.mmap(f.fileno(), 0)
    readline = buf.readline
    # Skip the header
    if handler.skip_header:
      while True:
        line = readline()
        if not line:
          return handler.line_count
        if not line.startswith(CSTYLE_COMMENT):
          # There is always a gap here, so we don't need to count it.
          break
    # Do the actual counting
    while True:
      line = readline()
      if not line:
        break
      handler.add(line)
  return handler.line_count

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
  f.extend(glob_files_in(recurse, 'tests'))
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
  f.extend(glob_files('java'))
  f.extend(glob('./scripts/*.cmake'))

  p = re.compile(r'(.+[\/])*[^\/]+\.hidden[^\/]+')
  f = [normpath(s) for s in f]
  return [s for s in f if not p.match(s)]

count = 0
for f in get_files():
  lines = mapcount(f)
  count += lines
print(f'Line Count: {count}\n')
