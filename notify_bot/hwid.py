from pathlib import Path
def read_hwid(path="config/hwid.txt"):
 try: return Path(path).read_text().strip() or None
 except FileNotFoundError: return None