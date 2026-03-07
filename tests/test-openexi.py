import os, sys
import traceback
from pathlib import Path
from glob import glob

EXI_CURR_DIR = Path(__file__).parent
sys.path.insert(0, str(EXI_CURR_DIR / 'exiconf'))
from exiconf.main import EXI_BASE_DIR, EXI_BIN_DIR, TEST_SRC_DIR

sys.path.insert(0, str(EXI_BASE_DIR / 'vendored' / 'xmldiff'))
from xmldiff.main import diff_texts
from xmldiff.actions import UpdateTextIn, UpdateTextAfter
from lxml import etree

# Returns true if xml is not different.
def diff_xml(name, file1, file2, parse_options=None) -> bool:
  try:
    bytes1 = str(file1).encode()
    bytes2 = str(file2).encode()
    diff_list = diff_texts(bytes1, bytes2, parse_options=parse_options)
    real_diffs = []
    # Fixup diffs with empty data
    # TODO: Add option to compare without preserves
    for diff in diff_list:
      if isinstance(diff, (UpdateTextIn, UpdateTextAfter)):
        old = diff.oldtext if diff.oldtext is not None else ''
        new = diff.text if diff.text is not None else ''
        if old.strip() != new.strip():
          real_diffs.append(diff)
      else:
        real_diffs.append(diff)
    if len(real_diffs) != 0:
      print(f'[diff] {Path(name).as_posix()}: {diff_list}')
      return False
    return True
  except Exception as e:
    print(f'[diff] {Path(name).as_posix()}*: {type(e).__name__}: {e}')
    return False

cwd = os.getcwd()

from exiconf.jvm.setup import start_jvm, get_jvm_path
start_jvm(jvm_args=['-ea'])
#start_jvm(jvm_args=['-ea', '-Dexicpp.loglevel=verbose'])
from exiconf.openexi import OpenEXICoder
from exiconf.logging import outs, errs, set_log_level
from exiconf.coder import _try_demangle, _json_dump

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

def run_all_files(mangled=None, do_print=False):
  all_files = list(glob(
    '**/*.xml',
    #'at/*.xml',
    root_dir=TEST_SRC_DIR,
    recursive=True
  ))

  logger = outs()
  print(logger.get_level())
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
  
  print('For:', handler.mangled, _json_dump(handler))
  print('  Equal:', eq_count)
  print('  Total:', len(all_files))
  print()

def run_all(mangled=None, do_print=False):
  if mangled is None:
    run_all_files(do_print=do_print)
    return
  # Actually run
  for m in mangled:
    run_all_files(mangled=m, do_print=do_print)

if __name__ == "__main__":
  set_log_level('info')
  #run_all_files(do_print=True)
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
