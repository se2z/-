import asyncio,json,threading,time
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from .models import AdminEvent,BackendResult
from .send_guard import SendGuard
from .backend import load_subscribers
from .formatting import validate_image
class Scheduler:
 def __init__(self,cfg,backend,per_recipient_interval=1.0,global_interval=.05):
  cfg.validate();self.c=cfg;self.backend=backend;self.workers=cfg.get("max_workers");self.pool=ThreadPoolExecutor(max_workers=self.workers);self.lock=threading.Lock();self.failures=0;self.queue_state="RUNNING";self.paused=set();self.sem=None;self.guard=SendGuard(per_recipient_interval,global_interval);self.closed=False
 def _flush(self,e):
  dest=self.c.get("event_sink") if self.c.is_set("event_sink") else "state/admin_events.jsonl";p=Path(dest);p.parent.mkdir(parents=True,exist_ok=True)
  with p.open("a",encoding="utf8") as f:f.write(json.dumps(e.__dict__)+"\n")
 def _reject(self,account,detail):
  p=Path("state/failed.jsonl");p.parent.mkdir(parents=True,exist_ok=True)
  with p.open("a",encoding="utf8") as f:f.write(json.dumps({"account_id":account,"detail":detail,"ts":time.time()})+"\n")
 def _failed(self,a,r):
  self._reject(a,r.detail)
  if r.ok is None:return
  with self.lock:
   self.failures+=1
   if self.failures>=self.c.get("failure_threshold"):
    self.failures=0;self.queue_state="RESTART_START";self._flush(AdminEvent("RESTART_START",a))
 def premium_expired(self,a):
  self.paused.add(a);e=AdminEvent("PREMIUM_EXPIRED",a)
  try:
   if self.c.is_set("admin_chat"):self.backend(self.c.get("admin_chat"),{"text":f"Premium expired: {a}"})
  finally:self._flush(e)
 def resume(self,a):self.paused.discard(a)
 def _send_sync(self,chat,payload,account_id="default",attempts=5):
  if account_id in self.paused:return BackendResult(False,"account paused")
  if isinstance(payload,dict) and (payload.get("image") or payload.get("photo")):
   try:validate_image(payload.get("image") or payload.get("photo"))
   except (OSError,ValueError) as exc:
    result=BackendResult(False,str(exc));self._failed(account_id,result);return result
  r=BackendResult(False,"no attempts")
  for i in range(max(1,min(attempts,10))):
   self.guard.wait(chat);r=self.backend(chat,payload)
   if r.ok is True:
    with self.lock:self.failures=0
    return r
   self._failed(account_id,r)
   if i+1<attempts:time.sleep(min(60,2**i))
  return r
 async def send(self,chat,payload,account_id="default",attempts=5):
  if self.sem is None:self.sem=asyncio.Semaphore(self.workers)
  async with self.sem:return await asyncio.to_thread(self._send_sync,chat,payload,account_id,attempts)
 async def broadcast(self,payload):return await asyncio.gather(*(self.send(c,payload) for c in load_subscribers()))
 async def run_async(self,jobs):
  try:return await asyncio.gather(*(self.send(*j) for j in jobs))
  finally:self.cleanup()
 def run_threads(self,jobs):
  try:return list(self.pool.map(lambda j:self._send_sync(*j),jobs))
  finally:self.cleanup()
 def cleanup(self):
  if not self.closed:self.pool.shutdown(wait=True);self.closed=True
