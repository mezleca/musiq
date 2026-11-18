#!/usr/bin/env python3

import os
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from endpoints import dispatch

class EndpointHandler(BaseHTTPRequestHandler):
    def respond(self, status: int, body: bytes = b"", headers: dict[str, str] | None = None) -> None:
        self.send_response(status)

        for name, value in (headers or {}).items():
            self.send_header(name, value)

        self.send_header("Content-Length", str(len(body)))
        self.end_headers()

        if body:
            self.wfile.write(body)

    def do_GET(self) -> None:
        dispatch(self, "GET", self.path, b"")

    def do_POST(self) -> None:
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length)
        dispatch(self, "POST", self.path, raw)

    def log_message(self, format: str, *args: object) -> None:
        pass

def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <tests-binary>", file=sys.stderr)
        return 2

    with ThreadingHTTPServer(("127.0.0.1", 0), EndpointHandler) as server:
        environment = os.environ.copy()
        environment["MUSIQ_TEST_API_URL"] = f"http://127.0.0.1:{server.server_port}/youtubei/v1"
        environment["MUSIQ_TEST_WEB_URL"] = f"http://127.0.0.1:{server.server_port}"
        serving = threading.Thread(target=server.serve_forever, daemon=True)
        serving.start()
        try:
            return subprocess.run([sys.argv[1], "[endpoint]"], env=environment, check=False).returncode
        finally:
            server.shutdown()
            serving.join()

if __name__ == "__main__":
    raise SystemExit(main())
