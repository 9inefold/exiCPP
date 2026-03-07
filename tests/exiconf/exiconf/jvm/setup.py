import os, sys
import _jpype, jpype
from pathlib import Path
from exiconf.main import EXI_BASE_DIR, EXI_BIN_DIR, TEST_SRC_DIR
from exiconf.logging import LogLevelArgs, Log, outs

__all__ = [
  'start_jvm',
  'get_jvm_path',
  'get_jvm_classpath',
  'get_jvm_extra_args',
]

JPYPE_DEFAULT_JVM_PATH = jpype.getDefaultJVMPath()
_used_jvm_path = None
_used_jvm_classpath = None
_used_jvm_extra_args = None

# Checks that the provided JVM file is valid
def _resolve_jvm(jvm_path: str, logger: Log) -> str:
  if jvm_path is None or len(str(jvm_path)) == 0:
    # No need to error, this is fine
    return JPYPE_DEFAULT_JVM_PATH
  
  # Use the provided path
  jvm_path = Path(str(jvm_path)).absolute()
  msg_start = f"Provided JVM path ('{jvm_path.as_posix()}')"
  msg_end = f"'{Path(JPYPE_DEFAULT_JVM_PATH).as_posix()}'"

  # Check it even exists
  if not jvm_path.exists():
    logger.warn(f"{msg_start} does not exist, using {msg_end}")
    return JPYPE_DEFAULT_JVM_PATH
  
  # Resolve symlink
  if jvm_path.is_symlink():
    try:
      jvm_path = jvm_path.resolve()
    except OSError as ex:
      logger.warn(f"{msg_start} failed to resolve: {ex}. Using {msg_end}")
      return JPYPE_DEFAULT_JVM_PATH
    msg_start = f"Resolved JVM path ('{jvm_path.as_posix()}')"
    # Check if resolved exists
    if not jvm_path.exists():
      logger.warn(f"{msg_start} does not exist, using {msg_end}")
      return JPYPE_DEFAULT_JVM_PATH
    
  # Check if possibly valid type
  if jvm_path.is_file():
    return str(jvm_path)
  logger.warn(f"{msg_start} is not a file, using {msg_end}")
  return JPYPE_DEFAULT_JVM_PATH

# Starts the JVM if not already running. Must be called before importing anything.
def start_jvm(jvm=None, classpath=None, jvm_args=None, logger:Log=None):
  if jpype.isJVMStarted():
    #logger.extra("JVM already running!")
    return
  
  if logger is None:
    logger = outs()
  
  jvm_path = _resolve_jvm(jvm, logger)
  global _used_jvm_path
  _used_jvm_path = jvm_path

  jvm_classpath = [
    (EXI_BASE_DIR/'bin/*').as_posix(),
    (EXI_BIN_DIR/'*').as_posix(),
  ]
  if classpath is not None:
    for clsp in classpath:
      clsp = Path(clsp).absolute().as_posix()
      jvm_classpath.append(clsp)
  global _used_jvm_classpath
  _used_jvm_classpath = jvm_classpath[:]

  jvm_extra_args = [
    '--add-opens=java.base/java.lang.reflect=ALL-UNNAMED',
    '--add-opens=java.xml/com.sun.org.apache.xalan.internal.xsltc.trax=ALL-UNNAMED',
    '--add-opens=java.xml/com.sun.org.apache.xalan.internal.xsltc.runtime=ALL-UNNAMED',
    #'--add-exports=java.base/jdk.internal.vm.annotation=ALL-UNNAMED',
  ]
  if jvm_args is not None:
    if type(jvm_args) is list:
      jvm_extra_args.extend(jvm_args)
    else:
      assert isinstance(jvm_args, str)
      jvm_extra_args.append(jvm_args)
  global _used_jvm_extra_args
  _used_jvm_extra_args = jvm_extra_args[:]

  _jpype.enableStacktraces(True)
  jpype.startJVM(jvm_path,
    *jvm_extra_args,
    classpath=[*jvm_classpath],
    convertStrings=False
  )

  if not jpype.isJVMStarted():
    print(f"JVM ('{Path(jvm_path).as_posix()}') could not be started!", flush=True)
    sys.exit(1)

# Gets the path to the JVM used with JPype
def get_jvm_path() -> str:
  if _used_jvm_path is None:
    return None
  return str(_used_jvm_path)

# Gets the classpath used with JPype
def get_jvm_classpath() -> list[str]:
  if _used_jvm_classpath is None:
    return None
  return _used_jvm_classpath[:]

# Gets the extra args used with JPype
def get_jvm_extra_args() -> list[str]:
  if _used_jvm_extra_args is None:
    return None
  return _used_jvm_extra_args[:]
