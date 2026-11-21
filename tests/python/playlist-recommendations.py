#!/usr/bin/env python3
"""Focused local browse-continuation contract test; no external credentials."""

import json
import os
import subprocess
import sys
import threading
from http.server import ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit
from server import EndpointHandler
from endpoints import dispatch
from endpoints.common import authenticated


def row(video, occurrence):
    return {"musicResponsiveListItemRenderer": {
        "flexColumns": [{"musicResponsiveListItemFlexColumnRenderer": {
            "text": {"runs": [{"text": video, "navigationEndpoint": {"watchEndpoint": {"videoId": video}}}]}
        }}],
        "playlistItemData": {"videoId": video, "playlistSetVideoId": occurrence},
    }}


class RecommendationsHandler(EndpointHandler):
    def do_POST(self):
        raw = self.rfile.read(int(self.headers["Content-Length"]))
        if urlsplit(self.path).path != "/youtubei/v1/browse":
            dispatch(self, "POST", self.path, raw)
            return
        request = json.loads(raw)
        query = parse_qs(urlsplit(self.path).query)
        if (urlsplit(self.path).path != "/youtubei/v1/browse"
                or request.get("browseId") not in {"VLPLrecommendations", "VLPLrecommendations-failure"}
                or "context" not in request or not authenticated(self)):
            self.send_error(400)
            return
        if query.get("continuation") == ["recommendations"]:
            assert query.get("ctoken") == ["recommendations"]
            assert query.get("type") == ["next"]
            if request["browseId"].endswith("-failure"):
                self.send_error(503)
                return
            response = {"continuationContents": {"sectionListContinuation": {"contents": [
                {"musicShelfRenderer": {"contents": [row("suggested", "to_be_updated_by_client")]}}
            ]}}}
        else:
            response = {"contents": {"twoColumnBrowseResultsRenderer": {
                "tabs": [{"tabRenderer": {"content": {"sectionListRenderer": {"contents": [{
                    "musicEditablePlaylistDetailHeaderRenderer": {
                        "editHeader": {"musicPlaylistEditHeaderRenderer": {"privacy": "PUBLIC"}},
                        "header": {"musicResponsiveHeaderRenderer": {"title": {"runs": [{"text": "Playlist"}]}}},
                    }
                }]}}}}],
                "secondaryContents": {"sectionListRenderer": {
                    "contents": [{"musicPlaylistShelfRenderer": {
                        "playlistId": request["browseId"][2:], "contents": [row("actual", "actual-occurrence")]
                    }}],
                    "continuations": [{"nextContinuationData": {"continuation": "recommendations"}}],
                }},
            }}}
        self.respond(200, json.dumps(response).encode(), {"Content-Type": "application/json"})


if __name__ == "__main__":
    with ThreadingHTTPServer(("127.0.0.1", 0), RecommendationsHandler) as server:
        environment = os.environ.copy()
        environment["MUSIQ_RECOMMENDATIONS_API_URL"] = f"http://127.0.0.1:{server.server_port}/youtubei/v1"
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            result = subprocess.run([sys.argv[1], "[playlist-recommendations-endpoint]"], env=environment, check=False)
        finally:
            server.shutdown()
            thread.join()
        raise SystemExit(result.returncode)
