import unittest,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
class Layout(unittest.TestCase):
 def test_flash_partitions(self):
  rows=[]
  for line in (ROOT/'partitions.csv').read_text().splitlines():
   if not line.strip() or line.lstrip().startswith('#'):continue
   name,kind,sub,off,size,*_=map(str.strip,line.split(','));rows.append((name,int(off,0),int(size,0)))
  names={name:(off,size) for name,off,size in rows}
  self.assertGreaterEqual(names['nvs'][1],256*1024)
  self.assertEqual(names['ota_0'][1],names['ota_1'][1])
  for (_,off,size),(_,nextoff,_) in zip(rows,rows[1:]):self.assertLessEqual(off+size,nextoff)
  self.assertLessEqual(rows[-1][1]+rows[-1][2],16*1024*1024)
  self.assertIn('zb_storage',names);self.assertIn('zb_fct',names)
 def test_native_profile_and_zero_retry(self):
  profile=(ROOT/'sdkconfig.defaults.esp32c6').read_text()
  for flag in ['CONFIG_ZB_ENABLED=y','CONFIG_ZB_ZCZR=y','CONFIG_ZB_RADIO_NATIVE=y','CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y']:self.assertIn(flag,profile)
  self.assertIn('kDefaultMaxCommandRetries = 0',(ROOT/'components/service/include/config_manager.hpp').read_text())
  self.assertNotIn('12345678',(ROOT/'main/app_main.cpp').read_text())
 def test_hardware_not_claimed(self):
  self.assertFalse(__import__('json').loads((ROOT/'docs/build-manifest.json').read_text())['hardware_verified'])
if __name__=='__main__':unittest.main()
