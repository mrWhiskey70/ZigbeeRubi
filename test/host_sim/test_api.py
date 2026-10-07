import unittest,os,signal
from support import Server,rule,report
class Api(unittest.TestCase):
 def setUp(self):self.s=Server()
 def tearDown(self):self.s.close()
 def test_create_nested_and_manual(self):
  r=rule();r['conditions']=[{'kind':'any','children':[1,2]},{'kind':'state','device_id':'0000000000000001','capability':'contact','op':'eq','value':True},{'kind':'state','device_id':'0000000000000002','capability':'occupancy','op':'eq','value':True}]
  self.assertEqual(self.s.call('/api/v1/scenarios',r)[0],201)
  self.assertEqual(self.s.call('/api/v1/scenarios')[1]['scenarios'][0],r)
  code,result=self.s.call('/api/v1/channels/power',{'device_id':'0000000000000003','channel_id':2,'on':True});self.assertEqual(code,202);self.assertEqual(self.s.call('/api/v1/operations/'+str(result['operation_id']))[1]['status'],'confirmed')
  self.assertEqual(self.s.call('/api/v1/nope')[0],404)
 def test_strict_errors_and_capacity(self):
  self.assertEqual(self.s.call('/api/v1/scenarios',{})[0],400)
  r=rule();r['actions'][0]['channel_id']=4;self.assertEqual(self.s.call('/api/v1/scenarios',r)[0],422)
  self.assertEqual(self.s.call('/api/v1/scenarios',raw=b' '*8193)[0],413)
  for i in range(1,25):r=rule();r['id']=i;self.assertEqual(self.s.call('/api/v1/scenarios',r)[0],201)
  r['id']=25;self.assertEqual(self.s.call('/api/v1/scenarios',r)[0],409)
  self.assertEqual(self.s.call('/api/v1/scenarios/1',method='DELETE')[0],200)
 def test_child_crash_is_503(self):
  self.assertGreater(self.s.info['child_pid'],1)
  os.kill(self.s.info['child_pid'],signal.SIGKILL)
  self.assertEqual(self.s.call('/api/v1/system')[0],503)
 def test_rename_persists(self):
  path='/api/v1/devices/0000000000000001';self.assertEqual(self.s.call(path,{'name':'Вход'},'PUT')[0],200)
  self.s.call('/api/v1/sim/restart',{})
  self.assertEqual(self.s.call('/api/v1/devices')[1]['devices'][0]['name'],'Вход')

 def test_deleted_device_and_timezone_survive_restart(self):
  self.assertEqual(self.s.call('/api/v1/devices/0000000000000001',method='DELETE')[0],200)
  self.assertEqual(self.s.call('/api/v1/settings',{'offset_minutes':60},'PUT')[0],200)
  self.s.call('/api/v1/sim/restart',{})
  self.assertEqual(len(self.s.call('/api/v1/devices')[1]['devices']),2)
  self.assertEqual(self.s.call('/api/v1/system')[1]['offset_minutes'],60)
 def test_parallel_reports_have_unique_sequences(self):
  import concurrent.futures
  def send(i):return self.s.call('/api/v1/sim/report',{'device_id':'0000000000000002','capability':'occupancy','value':i%2==0})
  with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:results=list(pool.map(send,range(20)))
  seq=[r[1]['sequence'] for r in results if r[1]['sequence']]
  self.assertEqual(len(seq),len(set(seq)))
  self.assertTrue(all(r[0]==200 for r in results))

 def test_pair_only_inside_join_window(self):
  self.assertEqual(self.s.call('/api/v1/sim/pair',{'kind':'door'})[0],422)
  self.assertEqual(self.s.call('/api/v1/network/join',{'seconds':1})[0],200)
  code,paired=self.s.call('/api/v1/sim/pair',{'kind':'door'});self.assertEqual(code,201)
  self.s.call('/api/v1/sim/advance',{'advance_ms':1001})
  self.assertEqual(self.s.call('/api/v1/sim/pair',{'kind':'motion'})[0],422)
  self.s.call('/api/v1/sim/restart',{})
  devices=self.s.call('/api/v1/devices')[1]['devices'];self.assertEqual(len(devices),4)
  self.assertEqual(devices[-1]['device_id'],paired['device_id'])
