import json, os, subprocess, sys, tempfile, urllib.request, urllib.error
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
class Server:
 def __init__(self, data=None):
  self.temp=tempfile.TemporaryDirectory() if data is None else None
  self.data=data or self.temp.name
  self.process=subprocess.Popen([sys.executable,str(ROOT/'host/server.py'),'--port','0','--data-dir',self.data,'--virtual-time'],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
  line=self.process.stdout.readline()
  if not line: raise RuntimeError(self.process.stderr.read())
  self.info=json.loads(line);self.url=self.info['url']
 def call(self,path,body=None,method=None,raw=None):
  payload=raw if raw is not None else (json.dumps(body).encode() if body is not None else None)
  req=urllib.request.Request(self.url+path,data=payload,method=method or ('POST' if payload is not None else 'GET'),headers={'Content-Type':'application/json'})
  try: response=urllib.request.urlopen(req,timeout=5)
  except urllib.error.HTTPError as e: response=e
  with response: return response.status,json.loads(response.read())
 def close(self):
  self.process.terminate()
  try:self.process.wait(timeout=5)
  except subprocess.TimeoutExpired:self.process.kill();self.process.wait()
  self.process.stdout.close();self.process.stderr.close()
  if self.temp:self.temp.cleanup()
def rule():
 return {'id':1,'name':'Свет при движении','enabled':True,'triggers':[{'device_id':'0000000000000002','kind':'occupancy.detected'}],'conditions':[],'actions':[{'kind':'set_channel_power','device_id':'0000000000000003','channel_id':1,'on':True},{'kind':'delay','delay_ms':60000},{'kind':'set_channel_power','device_id':'0000000000000003','channel_id':1,'on':False}]}
def report(s,value):return s.call('/api/v1/sim/report',{'device_id':'0000000000000002','capability':'occupancy','value':value})
def channel(s,index=0):return s.call('/api/v1/devices')[1]['devices'][2]['channels'][index]['power']
