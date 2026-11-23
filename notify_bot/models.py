from dataclasses import dataclass
from typing import Optional
@dataclass
class BackendResult:
    ok: Optional[bool]
    detail: str = ""
@dataclass
class Entity:
    type: str
    offset: int
    length: int
    url: Optional[str] = None
@dataclass
class AdminEvent:
    type: str
    account_id: str
    detail: str = ""