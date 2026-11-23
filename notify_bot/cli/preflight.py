from ..config import BuildConfig
from ..proxy_preflight import preflight
c=BuildConfig.load("config/build_config.json");c.validate();preflight();print("preflight passed; egress_ip=None; egress_verified=False")