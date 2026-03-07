import os, sys
import traceback
from pathlib import Path
from glob import glob

cwd = os.getcwd()
EXI_CURR_DIR = Path(__file__).parent
sys.path.insert(0, str(EXI_CURR_DIR / 'exiconf'))

from exiconf.jvm.setup import start_jvm, get_jvm_path
start_jvm(jvm_args=['-ea'])
#start_jvm(jvm_args=['-ea', '-Dexicpp.loglevel=verbose'])
from exiconf.constants import EXI_BASE_DIR, EXI_BIN_DIR, TEST_SRC_DIR
from exiconf.coder import _try_demangle, _json_dump
from exiconf.diff import diff_xml
from exiconf.logging import LogLevel, outs, errs, set_log_level
from exiconf.openexi import OpenEXICoder

def try_demangle(mangled=str):
  try:
    _try_demangle(mangled)
  except Exception:
    traceback.print_exc()

#try_demangle('COiPcdlipV10')
#try_demangle('_2OiPdpYej1skh1ajignh6ducoY')
#try_demangle('_0OcB10000')
#try_demangle('CN')
#try_demangle('yS')
#try_demangle('cB64M16V128')
#try_demangle('iPpYpb48ehb4fhzzq75zf35ugmuxqju16t51cfago4mdqczigi18fha1hcjxetkrem5uq3uuncjqct4geY')

OUT_DIR = EXI_BASE_DIR / 'tests/out'
if not OUT_DIR.exists():
  OUT_DIR.mkdir()

BAD_ITEMS = [str(Path(x)) for x in ['xml/012.xml']]

def do_roundtrip(test_path: Path, do_print = False):
  handler = OpenEXICoder()
  if do_print:
    relpath = test_path.relative_to(cwd).as_posix()
    print(relpath, ':', sep='', flush=True);
  test_path = Path(test_path)
  stem = str(test_path.stem)
  xml_in = test_path.read_text('utf8')
  data = handler.encode(xml_in)
  if data is None:
    return
  (OUT_DIR / f'{stem}.exi').write_bytes(data)
  xml_out = handler.decode(data)
  if xml_out is None:
    return
  (OUT_DIR / f'{stem}.xml').write_bytes(xml_out.encode('utf8'))
  if do_print:
    print(xml_out, '\n', flush=True)

def run_all_files(mangled=None, print_extra=False):
  all_files = list(glob(
    '**/*.xml',
    #'at/*.xml',
    root_dir=TEST_SRC_DIR,
    recursive=True
  ))

  logger = outs()
  handler = OpenEXICoder(mangled=mangled, logger=logger)
  err_dir = EXI_CURR_DIR/'out/err'
  if not err_dir.exists():
    err_dir.mkdir(parents=True)
  else:
    err_files = glob('*.xml', root_dir=err_dir)
    for f in err_files:
      os.remove(str(err_dir/f))

  eq_count = 0
  for f in all_files:
    is_eq = False
    is_bad = f in BAD_ITEMS
    xml_out = None
    try:
      xml_dir = Path(TEST_SRC_DIR / f).parent
      xml_in = (TEST_SRC_DIR / f).read_text('utf8')
      data = handler.encode(xml_in, f, xml_dir)
      if data is None:
        logger.extra(f'encoding {Path(f).as_posix()} failed', flush=True)
        continue
      xml_out = handler.decode(data, f)
      if xml_out is None:
        logger.extra(f'decoding {Path(f).as_posix()} failed', flush=True)
        continue
      if not is_bad:
        is_eq = diff_xml(f, xml_in, xml_out)
        # Print results
        if is_eq:
          logger.extra(f'{Path(f).as_posix()}: {is_eq}', flush=True)
      else:
        logger.extra(f'{Path(f).as_posix()}: Skipped', flush=True)
        
    except Exception as e:
      logger.extra(f'{Path(f).as_posix()}*: {type(e).__name__}: {e}', flush=True)
    # Check results
    if is_eq:
      eq_count += 1
    elif xml_out is not None:
      if is_bad:
        # Skipping, but add to the count anyways
        eq_count += 1
      (EXI_CURR_DIR/'out/err'/Path(f).name).write_text(xml_out)
  
  if print_extra:
    jdump = _json_dump(handler)
  else:
    jdump = ''
  logger.always('For:', handler.mangled, jdump)
  logger.always('Equal:', eq_count)
  logger.always('Total:', len(all_files), end='\n\n')

def run_all(mangled=None, print_extra=False):
  if mangled is None:
    run_all_files(print_extra=print_extra)
    return
  # Actually run
  for m in mangled:
    run_all_files(mangled=m, print_extra=print_extra)

if __name__ == "__main__":
  set_log_level('info')
  #run_all_files(print_extra=True)
  run_all([
    'iPcdip',
    'yPcdip',
    'pPcdip',
    'cPcdipB1000000V0',
  ])

  #CustomSAXParserFactory.printTypeOfParser()
  # TODO: Handle doctype
  
  #do_roundtrip(TEST_SRC_DIR / 'xml/008.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'xml/008r.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'xml/042.xml')
  #do_roundtrip(TEST_SRC_DIR / 'xml/066.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'xml/080.xml', do_print=True)

  #do_roundtrip(TEST_SRC_DIR / 'xml/070.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'at/at-01.xml', do_print=True)

  #do_roundtrip(TEST_SRC_DIR / 'me/HTML.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'me/HTML2.xml', do_print=True)
  #do_roundtrip(TEST_SRC_DIR / 'me/HTML3.xml', do_print=True)

  #do_roundtrip(TEST_SRC_DIR / 'me/Nested.xml')
  #do_roundtrip(TEST_SRC_DIR / 'me/CDATA2.xml')
  ##do_roundtrip(EXI_BASE_DIR / 'tests/nested-ent.hidden.xml')
  pass
