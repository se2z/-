import threading,time
class SendGuard:
 def __init__(self,per_recipient=1,global_interval=.05): self.lock=threading.Lock();self.last={};self.g=0;self.per=per_recipient;self.glob=global_interval
 def wait(self,c):
  with self.lock:
   n=time.monotonic();d=max(0,self.per-(n-self.last.get(c,0)),self.glob-(n-self.g));self.last[c]=n+d;self.g=n+d
  if d:time.sleep(d)