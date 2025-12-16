import argparse
import sys, os, subprocess
from subprocess import run as run_proc
from os.path import isfile, join

cwd = os.getcwd()

parser = argparse.ArgumentParser()
parser.add_argument('--java', help='the java runtime executable path')
parser.add_argument('--jar', help='the exificient jar file')
args = parser.parse_args()

def lprint(*args):
  print("[test]", *args)

def testrun(args):
  proc = run_proc(args, cwd=cwd, capture_output=True, text=True, check=True)
  if proc.returncode != 0 or proc.stdout.startswith("[ERROR]"):
    lprint(f"ERROR {proc.returncode}")

if __name__ == "__main__":
  invocation = f"{args.java} -jar {args.jar}"
  testrun(f"{invocation} -decode -i {cwd}/FileXYZ.exi -o {cwd}/FileXYZ.xml")
  #parser.print_help()
  #lprint(invocation, f"-i {cwd}/FileXYZ.exi -i {cwd}/FileXYZ.xml")
  sys.exit(1)
