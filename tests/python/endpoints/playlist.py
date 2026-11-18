from .common import Endpoint, authenticated

# playlist creation: https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/mixins/playlists.py#L255
# playlist edits: https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/mixins/playlists.py#L322
class PlaylistEndpoint(Endpoint):
    def validate(self, handler, request, query, raw):
        if not super().validate(handler, request, query, raw) or not authenticated(handler):
            return False

        if self.path.endswith("/playlist/create"):
            return request.get("title") == "Local playlist" and request.get("privacyStatus") == "PRIVATE"

        if request.get("playlistId") != "PLlocal-created":
            return False

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
    PlaylistEndpoint("POST", "/youtubei/v1/playlist/create", "playlist-create.json"),
    PlaylistEndpoint("POST", "/youtubei/v1/browse/edit_playlist", "playlist-edit.json"),
)
