import html
import re
from pathlib import Path
from .models import Entity
_MENTION = re.compile(r"@([A-Za-z0-9_]{5,})|tg://user\?id=(\d+)")

def find_mentions(text):
    return [
        {"kind": "username", "value": match.group(1)} if match.group(1)
        else {"kind": "user_id", "value": match.group(2)}
        for match in _MENTION.finditer(text)
    ]
def parse_text(text,style="plain",entities=(),custom_emoji_document_id=None):
 if style not in {"plain","markdown_v2","html"}: raise ValueError("invalid style")
 out=[]; pos={}; source_units=0; target_units=0
 for c in text:
  e=html.escape(c,quote=False) if style=="html" else (("\\"+c) if style=="markdown_v2" and c in r"_*-[]()~`>#+-=|{}.!\\" else c)
  source_len=len(c.encode("utf-16-le"))//2; target_len=len(e.encode("utf-16-le"))//2
  pos[source_units]=(target_units,target_len); source_units+=source_len; target_units+=target_len; out.append(e)
 pos[source_units]=(target_units,0)
 mapped=[]
 for x in entities:
  if x.type=="custom_emoji" and custom_emoji_document_id is None: continue
  a=x.offset;b=a+x.length
  if a not in pos or b not in pos: raise ValueError("entity range is not on UTF-16 character boundaries")
  mapped.append(Entity(x.type,pos[a][0],pos[b][0]-pos[a][0],x.url))
 return "".join(out),mapped
def validate_image(path,max_bytes=10000000):
 b=Path(path).read_bytes()
 if len(b)>max_bytes: raise ValueError("image too large")
 if not (b.startswith(bytes.fromhex("ffd8ff")) or b.startswith(bytes.fromhex("89504e470d0a1a0a")) or (b[:4]==b"RIFF" and b[8:12]==b"WEBP")): raise ValueError("bad signature")
 return True
