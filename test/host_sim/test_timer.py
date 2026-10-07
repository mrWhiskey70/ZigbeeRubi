import unittest
from support import Server,rule,report,channel
class Timer(unittest.TestCase):
 def setUp(self):self.s=Server()
 def tearDown(self):self.s.close()
 def test_motion_delay_and_manual_override(self):
  self.assertEqual(self.s.call('/api/v1/scenarios',rule())[0],201)
  report(self.s,True);self.assertIs(channel(self.s),True)
  self.s.call('/api/v1/sim/advance',{'advance_ms':59000});self.assertIs(channel(self.s),True)
  self.s.call('/api/v1/sim/advance',{'advance_ms':1000});self.assertIs(channel(self.s),False)
  report(self.s,False);report(self.s,True)
  self.s.call('/api/v1/channels/power',{'device_id':'0000000000000003','channel_id':1,'on':True})
  self.s.call('/api/v1/sim/advance',{'advance_ms':60000});self.assertIs(channel(self.s),True)
  self.assertTrue(any(x['reason']=='confirmed' for x in self.s.call('/api/v1/log')[1]['entries']))
 def test_unavailable_and_parallel_reports(self):
  self.s.call('/api/v1/scenarios',rule());self.s.call('/api/v1/sim/report',{'device_id':'0000000000000003','available':False});report(self.s,True);self.assertIsNone(channel(self.s))
  self.assertTrue(any(x['reason']=='output_unavailable' for x in self.s.call('/api/v1/log')[1]['entries']))
