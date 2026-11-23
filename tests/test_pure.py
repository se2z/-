import json,tempfile,unittest
from pathlib import Path
from notify_bot.config import BuildConfig
from notify_bot.hwid import read_hwid
from notify_bot.formatting import parse_text,validate_image
from notify_bot.models import Entity,BackendResult
from notify_bot.backend import load_subscribers
from notify_bot.scheduler import Scheduler
from notify_bot.license import issue,verify,revoke
import threading
from unittest.mock import patch
from notify_bot import proxy_preflight
class PureTests(unittest.TestCase):
 def test_config(self):BuildConfig.load("config/build_config.json").validate()
 def test_hwid_missing(self):self.assertIsNone(read_hwid("state/absent"))
 def test_html_entity_offsets(self):
  text,es=parse_text("<x>", "html", [Entity("bold",0,3)]);self.assertEqual(text,"&lt;x&gt;");self.assertEqual((es[0].offset,es[0].length),(0,9))
 def test_utf16_offsets_with_astral_character(self):
  text,es=parse_text("😀<", "html", [Entity("bold",2,1)]);self.assertEqual(text,"😀&lt;");self.assertEqual((es[0].offset,es[0].length),(2,4))
 def test_subscribers_need_consent_fields(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/"subs";p.write_text(json.dumps({"chat_id":1})+"\n"+json.dumps({"chat_id":2,"consent_ts":"now","source":"button"})+"\n");self.assertEqual(load_subscribers(p),[2])
 def test_dry_result_does_not_increment_counter(self):
  c=BuildConfig.load("config/build_config.json");s=Scheduler(c,lambda *_:BackendResult(None,"dry"));s._failed("a",BackendResult(None));self.assertEqual(s.failures,0);s.cleanup()
 def test_image_signature_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/"x";p.write_bytes(b"abc")
   with self.assertRaises(ValueError):validate_image(p)
 def test_http_connect_mock_handshake_and_preflight_order(self):
  class FakeSocket:
   def __init__(self,response):self.response=response;self.sent=b"";self.closed=False
   def sendall(self,data):self.sent+=data
   def recv(self,n):
    if not self.response:return b""
    out=self.response[:min(n,2)];self.response=self.response[len(out):];return out
   def close(self):self.closed=True
  sock=FakeSocket(b"HTTP/1.1 200 Connection established\r\nX: y\r\n\r\n")
  order=[]
  with patch.object(proxy_preflight,"preflight",side_effect=lambda:order.append("preflight")),patch.object(proxy_preflight.socket,"create_connection",side_effect=lambda *a:order.append("connect") or sock):
   result,rtt=proxy_preflight.http_connect("proxy",8080,"example.test",443,1)
  self.assertIs(result,sock);self.assertGreaterEqual(rtt,0);self.assertEqual(order,["preflight","connect"]);self.assertIn(b"CONNECT example.test:443",sock.sent)
 def test_socks5_mock_and_partial_reply_failure(self):
  class FakeSocket:
   def __init__(self,chunks):self.chunks=list(chunks);self.sent=b"";self.closed=False
   def sendall(self,data):self.sent+=data
   def recv(self,n):
    if not self.chunks:return b""
    item=self.chunks.pop(0);return item[:n]
   def close(self):self.closed=True
  good=FakeSocket([b"\x05",b"\x00",b"\x05\x00\x00\x01",b"\x7f\x00",b"\x00\x01\x01\xbb"])
  with patch.object(proxy_preflight.socket,"create_connection",return_value=good):
   sock,rtt=proxy_preflight.socks5_connect("proxy",1080,"example.test",443,1)
  self.assertIs(sock,good);self.assertGreaterEqual(rtt,0);self.assertIn(b"example.test",good.sent)
  partial=FakeSocket([b"\x05",b"\x00",b"\x05\x00"])
  with patch.object(proxy_preflight.socket,"create_connection",return_value=partial),self.assertRaises(OSError):proxy_preflight.socks5_connect("proxy",1080,"example.test",443,1)
  self.assertTrue(partial.closed)
 def test_license_issue_verify_revoke(self):
  with tempfile.TemporaryDirectory() as d:
   secret=Path(d)/"secret";secret.write_bytes(b"s"*32);revoked=Path(d)/"revoked.jsonl";token=issue("K-1","HW-1",secret)
   self.assertTrue(verify(token,"HW-1",secret,revoked));revoke("K-1",revoked);self.assertFalse(verify(token,"HW-1",secret,revoked))
 def test_failure_threshold_atomic(self):
  cfg=BuildConfig.load("config/build_config.json")
  with tempfile.TemporaryDirectory() as d:
   cfg.data["event_sink"]={"value":str(Path(d)/"events.jsonl"),"state":"configured"}
   scheduler=Scheduler(cfg,lambda *_:BackendResult(False,"fail"));threads=[threading.Thread(target=lambda:[scheduler._failed("acct",BackendResult(False,"fail")) for _ in range(100)]) for _ in range(8)]
   [t.start() for t in threads];[t.join() for t in threads]
   self.assertEqual(scheduler.failures,0);self.assertEqual(scheduler.queue_state,"RESTART_START");self.assertEqual(len(Path(d,"events.jsonl").read_text().splitlines()),160);scheduler.cleanup()
if __name__=="__main__":unittest.main()
