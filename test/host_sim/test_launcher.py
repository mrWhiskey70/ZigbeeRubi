"""A real browser boundary probe: --open must receive the bound HTTP URL."""
import json, os, subprocess, sys, tempfile, unittest, urllib.request
from pathlib import Path
from support import ROOT

@unittest.skipIf(os.name == 'nt', 'BROWSER command probe uses a POSIX executable')
class Launcher(unittest.TestCase):
 def test_custom_assets_and_browser_receive_live_url(self):
  with tempfile.TemporaryDirectory() as temp:
   d=Path(temp);assets=d/'assets';assets.mkdir();(assets/'index.html').write_text('portable asset probe')
   browser=d/'browser';record=d/'opened-url'
   browser.write_text('#!'+sys.executable+'\nimport sys\nfrom pathlib import Path\nPath('+repr(str(record))+').write_text(sys.argv[1])\n');browser.chmod(0o755)
   env=dict(os.environ,BROWSER=str(browser)+' %s')
   p=subprocess.Popen([sys.executable,str(ROOT/'host/server.py'),'--port','0','--data-dir',str(d/'data'),'--assets',str(assets),'--open'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
   try:
    line=p.stdout.readline();self.assertTrue(line, p.stderr.read() if p.poll() is not None else 'server did not announce its URL')
    info=json.loads(line)
    with urllib.request.urlopen(info['url'],timeout=5) as response:self.assertEqual(response.read(),b'portable asset probe')
    self.assertEqual(record.read_text(),info['url'])
   finally:
    p.terminate();p.wait(timeout=5);p.stdout.close();p.stderr.close()

class StaticAssets(unittest.TestCase):
 def test_module_mime_ignores_system_mapping(self):
  with tempfile.TemporaryDirectory() as temp:
   d=Path(temp);assets=d/'assets';assets.mkdir()
   (assets/'app.js').write_text('export const ready = true;')
   script='import sys, mimetypes; sys.path.insert(0, '+repr(str(ROOT/'host'))+'); mimetypes.init(); mimetypes.add_type("text/plain", ".js"); import server; server.main()'
   p=subprocess.Popen([sys.executable,'-c',script,'--port','0','--data-dir',str(d/'data'),'--assets',str(assets)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
   try:
    line=p.stdout.readline();self.assertTrue(line, p.stderr.read() if p.poll() is not None else 'server did not announce its URL')
    info=json.loads(line)
    with urllib.request.urlopen(info['url']+'/app.js',timeout=5) as response:
     self.assertEqual(response.headers.get_content_type(),'text/javascript')
     self.assertEqual(response.read(),b'export const ready = true;')
   finally:
    p.terminate();p.wait(timeout=5);p.stdout.close();p.stderr.close()
