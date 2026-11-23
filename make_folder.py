from pathlib import Path
for x in ("notify_bot/cli","config","state","tests"):Path(x).mkdir(parents=True,exist_ok=True)
Path("state/.gitkeep").touch()