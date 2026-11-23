import json,urllib.parse,urllib.request,secrets,mimetypes
from pathlib import Path
from .models import BackendResult
class TelegramBackend:
 def __init__(self,token_path="config/token.txt",timeout=15):
  self.token=Path(token_path).read_text(encoding="utf8").strip()
  if not self.token:raise ValueError("empty bot token")
  self.base="https://api.telegram.org/bot"+self.token+"/";self.timeout=timeout
 def __call__(self,chat_id,payload):
  if isinstance(payload,str):payload={"text":payload}
  try:
   if payload.get("image") or payload.get("photo"):
    method="sendPhoto"; image=Path(payload.get("image") or payload.get("photo")); boundary="----notify"+secrets.token_hex(12)
    fields={"chat_id":str(chat_id),"photo":"attach://image","caption":payload.get("text","")}; chunks=[]
    for k,v in fields.items(): chunks.append(("--"+boundary+"\r\nContent-Disposition: form-data; name=\""+k+"\"\r\n\r\n"+str(v)+"\r\n").encode())
    mime=mimetypes.guess_type(image.name)[0] or "application/octet-stream"
    chunks += [("--"+boundary+"\r\nContent-Disposition: form-data; name=\"image\"; filename=\""+image.name+"\"\r\nContent-Type: "+mime+"\r\n\r\n").encode(),image.read_bytes(),("\r\n--"+boundary+"--\r\n").encode()]
    data=b"".join(chunks); headers={"Content-Type":"multipart/form-data; boundary="+boundary}; method="sendPhoto"
   else:
    method="sendMessage"; body={"chat_id":chat_id,"text":payload.get("text","")}
    for k in ("parse_mode","entities","disable_notification","protect_content"):
     if k in payload:body[k]=json.dumps(payload[k],ensure_ascii=False) if k=="entities" else payload[k]
    data=urllib.parse.urlencode(body).encode();headers={"Content-Type":"application/x-www-form-urlencoded"}
   req=urllib.request.Request(self.base+method,data=data,headers=headers)
   with urllib.request.urlopen(req,timeout=self.timeout) as response:data=json.loads(response.read().decode("utf8"))
   return BackendResult(data.get("ok") is True,str(data.get("description","")))
  except Exception as exc:return BackendResult(False,str(exc))
def load_subscribers(path="state/subscribers.jsonl"):
 p=Path(path)
 if not p.exists():return []
 rows=[]
 for line in p.read_text(encoding="utf8").splitlines():
  if not line:continue
  row=json.loads(line)
  if all(k in row for k in ("chat_id","consent_ts","source")) and row["consent_ts"] and row["source"]:rows.append(row["chat_id"])
 return rows
