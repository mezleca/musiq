import json
from urllib.parse import parse_qs, urlsplit

from .common import Endpoint, authenticated, FIXTURES

# https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/mixins/browsing.py#L115
# https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/mixins/library.py#L21
# https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/mixins/playlists.py#L144
# https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/continuations.py#L77
class BrowseEndpoint(Endpoint):
    def validate(self, handler, request, query, raw):
        if not super().validate(handler, request, query, raw):
            return False
        browse_id = request.get("browseId")
        if browse_id == "FEmusic_liked_playlists" and not authenticated(handler):
            return False
        if browse_id is not None:
            return browse_id in {"FEmusic_home", "FEmusic_liked_playlists", "VLPLlocal-created"}
        token = query.get("continuation", request.get("continuation", [""]))
        if isinstance(token, list):
            token = token[0] if token else ""
        return token in {"browse-token", "playlist-token"} and query.get("type") == ["next"]

    def handle(self, handler, url, raw):
        query = parse_qs(urlsplit(url).query)
        request = json.loads(raw or b"{}")
        if not self.validate(handler, request, query, raw):
            handler.send_error(400)
            return
        token = query.get("continuation", [""])[0]
        fixture = "playlist-continuation.json" if token == "playlist-token" else (
            "playlist.json" if request.get("browseId") == "VLPLlocal-created" else self.fixture
        )
        handler.respond(200, (FIXTURES / fixture).read_bytes(), {"Content-Type": "application/json"})

ENDPOINTS = (BrowseEndpoint("POST", "/youtubei/v1/browse", "browse.json"),)
