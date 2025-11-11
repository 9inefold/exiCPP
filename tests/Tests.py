import argparse
import sys, os
from os.path import isfile, join

cwd = os.getcwd()

parser = argparse.ArgumentParser()
parser.add_argument('--java', help='the java runtime executable path')
parser.add_argument('--jar', help='the exificient jar file')
args = parser.parse_args()

if __name__ == "__main__":
  #parser.print_help()
  print(f"{args.java} -jar {args.jar} {cwd}/FileXYZ.exi")
  sys.exit(1)
