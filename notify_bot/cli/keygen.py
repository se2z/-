import argparse,secrets,sys
from pathlib import Path
from ..license import issue,verify,revoke
from ..hwid import read_hwid
p=argparse.ArgumentParser();p.add_argument("--config",default="config/build_config.json");p.add_argument("action",choices=["new-secret","issue","verify","revoke"]);p.add_argument("value",nargs="?");a=p.parse_args()
if a.action=="new-secret":Path("config/license_secret.bin").write_bytes(secrets.token_bytes(32));print("secret created")
elif a.action=="issue":
 hwid=read_hwid()
 if not hwid:raise SystemExit("HWID missing: set config/hwid.txt first")
 print(issue(a.value or secrets.token_hex(16),hwid))
elif a.action=="verify":print(verify(a.value or "",read_hwid()))
else:revoke(a.value or "");print("revoked")
