import hmac,hashlib,json,time
from pathlib import Path
def issue(key,hwid,secret_path="config/license_secret.bin"):
 b=json.dumps({"key":key,"hwid":hwid},sort_keys=True,separators=(",",":")).encode(); return b.hex()+"."+hmac.new(Path(secret_path).read_bytes(),b,hashlib.sha256).hexdigest()
def verify(token,hwid,secret_path="config/license_secret.bin",revoked_path="state/revoked.jsonl"):
 try:
  b,s=token.split(".",1); raw=bytes.fromhex(b); o=json.loads(raw); good=hmac.compare_digest(s,hmac.new(Path(secret_path).read_bytes(),raw,hashlib.sha256).hexdigest()) and o["hwid"]==hwid
  p=Path(revoked_path); return good and not (p.exists() and any(json.loads(x).get("key")==o["key"] for x in p.read_text().splitlines() if x))
 except (ValueError,KeyError,OSError): return False
def revoke(key,path="state/revoked.jsonl"):
 p=Path(path);p.parent.mkdir(parents=True,exist_ok=True)
 with p.open("a",encoding="utf8") as f:f.write(json.dumps({"key":key,"ts":int(time.time())})+"\n")
