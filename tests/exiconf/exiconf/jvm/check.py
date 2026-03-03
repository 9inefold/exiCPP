import sys, traceback
from exiconf.jvm.setup import _used_jvm_path

if _used_jvm_path is None:
  print("Expected JVM to be started!", flush=True)
  print(''.join(traceback.format_stack()[:-1]))
  sys.exit(1)
