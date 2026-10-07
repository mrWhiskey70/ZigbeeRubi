import unittest
from support import Server,rule,report,channel
class Restart(unittest.TestCase):
 def test_restart_unknown_no_pending(self):
  s=Server();other=Server()
  try:
   s.call('/api/v1/scenarios',rule());report(s,True)
   s.call('/api/v1/sim/restart',{});self.assertEqual(len(s.call('/api/v1/scenarios')[1]['scenarios']),1)
   self.assertIsNone(channel(s));self.assertEqual(s.call('/api/v1/system')[1]['pending'],0)
   report(s,True);self.assertIsNone(channel(s));s.call('/api/v1/sim/advance',{'advance_ms':90000});self.assertIsNone(channel(s))
   self.assertEqual(other.call('/api/v1/scenarios')[1]['scenarios'],[])
  finally:s.close();other.close()
