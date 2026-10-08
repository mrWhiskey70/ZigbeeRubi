"""Local HTTP bridge to a persistent C++ scenario runtime (stdlib only)."""
import argparse
import json
import mimetypes
import os
import queue
import subprocess
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
ASSET_TYPES = {'.html':'text/html; charset=utf-8',
               '.js':'text/javascript; charset=utf-8',
               '.css':'text/css; charset=utf-8',
               '.webmanifest':'application/manifest+json; charset=utf-8',
               '.png':'image/png'}

class Bridge:
    def __init__(self, binary, data, virtual):
        args = [str(binary), str(data)] + ([] if virtual else ['--real-time'])
        self.child = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                      stderr=None, text=True, encoding='utf-8', bufsize=1,
                                      creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        self.lock = threading.Lock()
        self.responses = queue.Queue()
        self.closed = threading.Event()
        threading.Thread(target=self.read, daemon=True).start()
        if not virtual:
            threading.Thread(target=self.heartbeat, daemon=True).start()

    def read(self):
        for line in self.child.stdout:
            try: self.responses.put(json.loads(line))
            except json.JSONDecodeError: self.responses.put(None)
        self.responses.put(None)

    def request(self, method, path, body=''):
        with self.lock:
            if self.child.poll() is not None:
                raise RuntimeError('C++ runtime exited')
            try:
                self.child.stdin.write(json.dumps({'method':method,'path':path,'body':body}, ensure_ascii=False)+'\n')
                self.child.stdin.flush()
                response = self.responses.get(timeout=5)
            except (OSError, queue.Empty) as e:
                self.child.terminate()
                raise RuntimeError('C++ runtime unavailable') from e
            if response is None: raise RuntimeError('C++ runtime unavailable')
            return response

    def heartbeat(self):
        while not self.closed.wait(.1):
            try: self.request('GET', '/api/v1/system')
            except RuntimeError: return

    def close(self):
        self.closed.set()
        if self.child.poll() is None: self.child.terminate()
        try: self.child.wait(timeout=3)
        except subprocess.TimeoutExpired: self.child.kill();self.child.wait()
        self.child.stdin.close();self.child.stdout.close()


def main(argv=None, on_ready=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('--host',default='127.0.0.1')
    parser.add_argument('--port',type=int,default=8080)
    parser.add_argument('--data-dir',type=Path,default=ROOT/'.sim-data')
    default_binary = ROOT/'build-simulator'/('zigbee_hub_sim.exe' if os.name == 'nt' else 'zigbee_hub_sim')
    parser.add_argument('--binary',type=Path,default=Path(os.environ.get('ZIGBEERUBI_SIM_BINARY',str(default_binary))))
    parser.add_argument('--assets',type=Path,default=ROOT/'components/web_ui/assets')
    parser.add_argument('--open',action='store_true',help='Open the interface in the default browser')
    parser.add_argument('--virtual-time',action='store_true')
    args=parser.parse_args(argv)
    assets=args.assets
    class Handler(BaseHTTPRequestHandler):
        def send_json(self,status,body):
            data=json.dumps(body,ensure_ascii=False).encode('utf-8')
            self.send_response(status);self.send_header('Content-Type','application/json; charset=utf-8')
            self.send_header('Content-Length',str(len(data)));self.send_header('Cache-Control','no-store')
            self.end_headers();self.wfile.write(data)
        def handle_api(self):
            route=urlsplit(self.path).path
            if self.command=='GET' and not route.startswith('/api/'):
                path=assets/('index.html' if route=='/' else route.lstrip('/'))
                if not path.resolve().is_relative_to(assets.resolve()) or not path.is_file():
                    self.send_json(404,{'error':'not_found'});return
                data=path.read_bytes();self.send_response(200)
                self.send_header('Content-Type',ASSET_TYPES.get(path.suffix.lower()) or mimetypes.guess_type(path)[0] or 'application/octet-stream')
                self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
            try:
                size=int(self.headers.get('Content-Length','0'))
                if size<0: raise ValueError()
            except ValueError:
                self.send_json(400,{'error':'invalid_length'});return
            if size>8192:
                self.close_connection=True;self.send_json(413,{'error':'too_large'});return
            try: body=self.rfile.read(size).decode('utf-8')
            except UnicodeDecodeError:
                self.send_json(400,{'error':'invalid_utf8'});return
            try: response=bridge.request(self.command,route,body)
            except RuntimeError:
                self.send_json(503,{'error':'runtime_unavailable'});return
            self.send_json(response['status'],response['body'])
        do_GET=handle_api;do_POST=handle_api;do_PUT=handle_api;do_DELETE=handle_api
        def log_message(self,*args):pass
    server=ThreadingHTTPServer((args.host,args.port),Handler)
    bridge=None
    try:
        bridge=Bridge(args.binary,args.data_dir,args.virtual_time)
        info={'url':f'http://{args.host}:{server.server_port}','child_pid':bridge.child.pid}
        if on_ready is None:print(json.dumps(info),flush=True)
        else:on_ready(info)
        if args.open:webbrowser.open(info['url'])
        server.serve_forever()
    finally:
        server.server_close()
        if bridge is not None:bridge.close()

if __name__=='__main__':main()
