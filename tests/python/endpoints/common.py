import json
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

FIXTURES = Path(__file__).parent.parent / "fixtures"

@dataclass(frozen=True)
class Endpoint:
    method: str
    path: str
    fixture: str | None = None

    def matches(self, method: str, url: str) -> bool:
        return method == self.method and urlsplit(url).path == self.path

    def handle(self, handler, url: str, raw: bytes) -> None:
        parsed = urlsplit(url)
        request = json.loads(raw or b"{}")
        if not self.validate(handler, request, parse_qs(parsed.query), raw):
            handler.send_error(400)
            return
        if self.fixture is None:
            handler.respond(200)
            return
        handler.respond(200, (FIXTURES / self.fixture).read_bytes(), {"Content-Type": "application/json"})

    def validate(self, handler, request: dict, query: dict[str, list[str]], raw: bytes) -> bool:
        if self.path.startswith("/youtubei/v1/"):
            if "context" not in request or not handler.headers.get("X-YouTube-Client-Name"):
                return False
            if query.get("prettyPrint") != ["false"]:
                return False
        return True

def authenticated(handler) -> bool:
    return (
        "SAPISID=local-secret" in handler.headers.get("Cookie", "")
        and handler.headers.get("Authorization", "").startswith("SAPISIDHASH ")
    )
