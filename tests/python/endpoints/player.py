from .common import Endpoint

# https://github.com/SimoHypers/limusic/blob/f82fc1ec0fe1e12ea023cb6da1550a911aa80eaf/crates/innertube/src/endpoints.rs#L50
class PlayerEndpoint(Endpoint):
    def validate(self, handler, request, query, raw):
        return super().validate(handler, request, query, raw) and request.get("videoId") == "video-id"

ENDPOINTS = (PlayerEndpoint("POST", "/youtubei/v1/player", "player.json"),)
