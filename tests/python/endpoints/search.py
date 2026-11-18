from .common import Endpoint

# https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/mixins/search.py#L164
class SearchEndpoint(Endpoint):
    def validate(self, handler, request, query, raw):
        return super().validate(handler, request, query, raw) and request.get("query") == "search query"


# https://github.com/sigma67/ytmusicapi/blob/488dbf31d8f9173331c80375fdc2fb6de6d45f6d/ytmusicapi/mixins/search.py#L381
class SuggestionsEndpoint(Endpoint):
    def validate(self, handler, request, query, raw):
        return super().validate(handler, request, query, raw) and request.get("input") == "seed query"

ENDPOINTS = (
    SearchEndpoint("POST", "/youtubei/v1/search", "search.json"),
    SuggestionsEndpoint("POST", "/youtubei/v1/music/get_search_suggestions", "search-suggestions.json"),
)
