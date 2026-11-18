from .common import Endpoint

# https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/mixins/watch.py#L112
class QueueEndpoint(Endpoint):
    def validate(self, handler, request, query, raw):
        if not super().validate(handler, request, query, raw):
            return False
        return request.get("videoId") == "video-id" or request.get("continuation") == "queue-token"

ENDPOINTS = (QueueEndpoint("POST", "/youtubei/v1/next", "next.json"),)
