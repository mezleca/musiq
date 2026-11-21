from .common import Endpoint, authenticated

# library removal: https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/mixins/library.py
class PlaylistUnsaveEndpoint(Endpoint):
    def validate(self, handler, request, query, raw):
        return (
            super().validate(handler, request, query, raw)
            and authenticated(handler)
            and set(request) == {"context", "target"}
            and request["target"] in [
                {"playlistId": "PLlocal-saved"}, {"playlistId": "PLlocal-refused"},
                {"playlistId": "PLlocal-gated"}, {"playlistId": "PLlocal-malformed"}
            ]
        )

    def response_body(self, handler, request, query):
        if request["target"]["playlistId"] == "PLlocal-refused":
            return b'{"status": "STATUS_FAILED"}'
        if request["target"]["playlistId"] == "PLlocal-gated":
            return b'{"actions": [{"showEngagementPanelEndpoint": {}}]}'
        if request["target"]["playlistId"] == "PLlocal-malformed":
            return b'[]'
        return b'{"responseContext": {}}'

# playlist deletion: https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/mixins/playlists.py#L423
class PlaylistDeleteEndpoint(Endpoint):
    def validate(self, handler, request, query, raw):
        return (
            super().validate(handler, request, query, raw)
            and authenticated(handler)
            and set(request) == {"context", "playlistId"}
            and request["playlistId"] in {"PLlocal-created", "PLlocal-refused", "PLlocal-no-status"}
        )

    def response_body(self, handler, request, query):
        playlist_id = request["playlistId"]
        if playlist_id == "PLlocal-refused":
            return b'{"status": "STATUS_FAILED"}'
        if playlist_id == "PLlocal-no-status":
            return b'{"responseContext": {}}'
        return super().response_body(handler, request, query)

# playlist creation and edits: https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/mixins/playlists.py#L255
# cover removal: https://github.com/SimoHypers/limusic/blob/f82fc1ec0fe1e12ea023cb6da1550a911aa80eaf/crates/innertube/src/endpoints.rs#L1334
class PlaylistEndpoint(Endpoint):
    def validate(self, handler, request, query, raw):
        if not super().validate(handler, request, query, raw) or not authenticated(handler):
            return False

        if self.path.endswith("/playlist/create"):
            return request.get("title") == "Local playlist" and request.get("privacyStatus") == "PRIVATE"

        if request.get("playlistId") != "PLlocal-created":
            return False

        if request.get("actions") == [{
            "action": "ACTION_REMOVE_CUSTOM_THUMBNAIL",
            "deletedCustomThumbnail": {
                "name": "studio_square_thumbnail",
                "type": "PLAYLIST_IMAGE_TYPE_CUSTOM_THUMBNAIL",
            },
        }]:
            return True

        for action in request.get("actions", []):
            kind = action.get("action")
            if kind == "ACTION_ADD_VIDEO" and action.get("addedVideoId") == "video-id":
                return True
            if kind == "ACTION_REMOVE_VIDEO" and action.get("removedVideoId") == "video-id" and action.get("setVideoId") == "set-video-id":
                return True
            if kind == "ACTION_SET_PLAYLIST_NAME" and action.get("playlistName") == "Renamed playlist":
                continue
            if kind == "ACTION_SET_PLAYLIST_DESCRIPTION" and action.get("playlistDescription") == "Local description":
                continue
            if kind == "ACTION_SET_PLAYLIST_PRIVACY" and action.get("playlistPrivacy") == "PUBLIC":
                continue
            if kind == "ACTION_SET_CUSTOM_THUMBNAIL" and action.get("addedCustomThumbnail", {}).get("playlistScottyEncryptedBlobId") == "local-blob":
                continue

        actions = {action.get("action") for action in request.get("actions", [])}
        return actions == {
            "ACTION_SET_PLAYLIST_NAME",
            "ACTION_SET_PLAYLIST_DESCRIPTION",
            "ACTION_SET_PLAYLIST_PRIVACY",
            "ACTION_SET_CUSTOM_THUMBNAIL",
        }

ENDPOINTS = (
    PlaylistUnsaveEndpoint("POST", "/youtubei/v1/like/removelike"),
    PlaylistEndpoint("POST", "/youtubei/v1/playlist/create", "playlist-create.json"),
    PlaylistDeleteEndpoint("POST", "/youtubei/v1/playlist/delete", "playlist-delete.json"),
    PlaylistEndpoint("POST", "/youtubei/v1/browse/edit_playlist", "playlist-edit.json"),
)
