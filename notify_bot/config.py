import json
from pathlib import Path
class BuildConfig:
 def __init__(self,d): self.data=d
 @classmethod
 def load(cls,p): return cls(json.loads(Path(p).read_text(encoding="utf8")))
 def get(self,k): return self.data[k]["value"]
 def is_set(self,k): return self.data[k]["state"]!="unset" and self.get(k) is not None
 def validate(self):
  if type(self.get("failure_threshold")) is not int or not 3<=self.get("failure_threshold")<=7: raise ValueError("failure_threshold 3..7")
  if type(self.get("max_workers")) is not int or not 1<=self.get("max_workers")<=256: raise ValueError("max_workers 1..256")
  if type(self.get("connect_timeout_ms")) is not int or self.get("connect_timeout_ms")<=0: raise ValueError("connect_timeout_ms positive")