import argparse, sys, os
from pathlib import Path

FILE_START = """
//===----------------------------------------------------------------------===//
//       Generated file, do not edit!
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
//     limitations under the License.
//
//===----------------------------------------------------------------------===//

""".lstrip()

def get_prologue_epilogue() -> tuple[str, str]:
  ELF_PROLOGUE = r"""
asm(
".pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n"
".byte 4\n"
  """.lstrip()
  ELF_EPILOGUE = r"""
".byte 0\n"
".popsection\n"
);
  """

  COFF_PROLOGUE = r"""
asm(
".section .debug_gdb_scripts, \"dr\"\n"
".byte 4\n"
  """.lstrip()
  COFF_EPILOGUE = r"""
".byte 0\n"
);
  """

  if os.name != 'nt':
    return ELF_PROLOGUE, ELF_EPILOGUE
  else:
    return COFF_PROLOGUE, COFF_EPILOGUE

PROLOGUE, EPILOGUE = get_prologue_epilogue()

def make_c_escape():
  import string
  tt = {
    '\\': "\\\\",
    #'?' : "\\?",
    #'\'': "\\'",
    '"' : "\\\"",
    #'\a': "\\a",
    '\b': "\\b",
    '\f': "\\f",
    '\n': "\\n",
    '\r': "\\r",
    '\t': "\\t",
    #'\v': "\\v",
  }
  for c in range(255):
    if chr(c) in tt or chr(c) in string.printable:
      continue
    tt[chr(c)] = f'\\{c:02x}'
  return str.maketrans(tt)

c_trans_table = make_c_escape()

def encode_line(line: str) -> str:
  first = line.translate(c_trans_table)
  second = f'.ascii "{first}"\n'.translate(c_trans_table)
  return f'"{second}"'

def is_empty_line(line: str) -> bool:
  line = line.strip()
  return len(line) == 0 or line.startswith('#')

def generate_asm_decl(filename: str, data: str) -> str:
  lines = [encode_line(filename + '\n')]
  for line in data.splitlines(keepends=True):
    if is_empty_line(line):
      continue
    line = line.replace('\t', '  ')
    lines.append(encode_line(line))
  # Empty file?
  if len(lines) <= 1:
    return ''
  return PROLOGUE + '\n'.join(lines) + EPILOGUE

def generate_gdb_embed(inputs: list[Path]) -> str:
  decls = []
  for input in inputs:
    try:
      data = input.read_text('ascii')
      decls.append(generate_asm_decl(input.name, data))
    except:
      print(f"failed to handle '{input.name}'")
  return FILE_START + '\n'.join(decls)

################################################################################
## Command-line Interface
################################################################################

def get_arg_parser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(prog='generate_gdb_embed')
  parser.add_argument(
    'output',
    help='the name of the outputted file',
    metavar='output-file',
    type=Path
  )
  parser.add_argument(
    'inputs',
    action='extend',
    nargs='+',
    type=_realpath, default=[],
    metavar='source',
    help='the input files'
  )
  return parser

def _realpath(input) -> Path:
  input = Path(str(input))
  if not input.exists():
    raise argparse.ArgumentTypeError(f"'{input.as_posix()}' does not exist!")
  return input

def parse_args() -> argparse.Namespace:
  args = sys.argv[1:]
  print(' '.join(args))
  return get_arg_parser().parse_args(args)

if __name__ == '__main__':
  args = parse_args()
  filedata = generate_gdb_embed(args.inputs)
  outpath = Path(args.output)
  if not outpath.parent.exists():
    outpath.parent.mkdir(parents=True)
  outpath.write_text(filedata, 'ascii')

################################################################################
## Test
################################################################################

def test_encode():
  input = """
class test_cmd (gdb.Command):
  def __init__ (self):
    # Some comment
    super (test_cmd, self).__init__ ("test-cmd", gdb.COMMAND_OBSCURE)
  def invoke (self, arg, from_tty):
    print ("test-cmd output, arg = %s" % arg)
test_cmd ()
  """.lstrip()

  output = r"""
asm(
".pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n"
".byte 4\n"
".ascii \"gdb.inlined-script\\n\"\n"
".ascii \"class test_cmd (gdb.Command):\\n\"\n"
".ascii \"  def __init__ (self):\\n\"\n"
".ascii \"    super (test_cmd, self).__init__ (\\\"test-cmd\\\", gdb.COMMAND_OBSCURE)\\n\"\n"
".ascii \"  def invoke (self, arg, from_tty):\\n\"\n"
".ascii \"    print (\\\"test-cmd output, arg = %s\\\" % arg)\\n\"\n"
".ascii \"test_cmd ()\\n\"\n"
".byte 0\n"
".popsection\n"
);
  """.lstrip()

  generated = generate_asm_decl('gdb.inlined-script', input)
  if generated.strip() == output.strip():
    print("Test PASSED!")
  else:
    print("Test FAILED!")
    #print(len(generated), len(output))
    #print(generated.encode("ascii"))
    #print(output.encode("ascii"))
    #print(f'`\n{generated}\n`')
    #print(f'`\n{output}\n`')
