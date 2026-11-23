from ..config import BuildConfig
c=BuildConfig.load("config/build_config.json");c.validate()
from pathlib import Path
items=[("LICENSE KEY","license_key"),("HWID",None),("PROXY","proxy"),("TOKEN",None),("TOTP","totp")]
for i,(name,key) in enumerate(items,1):
 state=("configured" if Path("config/hwid.txt").exists() and Path("config/hwid.txt").read_text().strip() else "unset") if name=="HWID" else (("configured" if Path("config/token.txt").exists() and Path("config/token.txt").read_text().strip() else "unset") if name=="TOKEN" else c.data[key]["state"])
 print(f"{i:02d} {name} | state={state}")
