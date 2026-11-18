from .account import ENDPOINTS as ACCOUNT
from .browse import ENDPOINTS as BROWSE
from .common import FIXTURES, Endpoint
from .player import ENDPOINTS as PLAYER
from .playlist import ENDPOINTS as PLAYLIST
from .queue import ENDPOINTS as QUEUE
from .search import ENDPOINTS as SEARCH
from .upload import ENDPOINTS as UPLOAD
from .visitor import ENDPOINTS as VISITOR

ENDPOINTS = (*ACCOUNT, *BROWSE, *PLAYER, *PLAYLIST, *QUEUE, *SEARCH, *UPLOAD, *VISITOR)

def dispatch(handler, method: str, url: str, body: bytes) -> None:
    for endpoint in ENDPOINTS:
        if endpoint.matches(method, url):
            endpoint.handle(handler, url, body)
            return
    handler.send_error(404)
