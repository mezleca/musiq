import json

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
            return browse_id in {"FEmusic_home", "FEmusic_liked_playlists", "VLPLlocal-created", "VLPLlocal-pending"}
        token = query.get("continuation", request.get("continuation", [""]))
        if isinstance(token, list):
            token = token[0] if token else ""
        return token in {"browse-token", "playlist-token"} and query.get("type") == ["next"]

    def response_body(self, handler, request, query):
        browse_id = request.get("browseId")
        if browse_id == "VLPLlocal-pending":
            attempt = getattr(handler.server, "playlist_browse_attempts", 0) + 1
            handler.server.playlist_browse_attempts = attempt

            # the first read is empty and subsequent reads expose which attempt was published
            if attempt == 1:
                return b'{"responseContext": {}}'
            response = json.loads((FIXTURES / "playlist.json").read_bytes())
            title = response["header"]["musicResponsiveHeaderRenderer"]["title"]
            title["runs"][0]["text"] = f"Playlist attempt {attempt}"
            return json.dumps(response).encode()

        if query.get("continuation") == ["playlist-token"]:
            return (FIXTURES / "playlist-continuation.json").read_bytes()
        if browse_id == "VLPLlocal-created":
            return (FIXTURES / "playlist.json").read_bytes()
        return super().response_body(handler, request, query)

ENDPOINTS = (BrowseEndpoint("POST", "/youtubei/v1/browse", "browse.json"),)
