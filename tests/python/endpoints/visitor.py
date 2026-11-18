# https://github.com/SimoHypers/limusic/blob/f82fc1ec0fe1e12ea023cb6da1550a911aa80eaf/crates/innertube/src/transport.rs#L628
class VisitorEndpoint:
    method = "GET"
    path = "/sw.js_data"

    def matches(self, method, url):
        from urllib.parse import urlsplit
        return method == self.method and urlsplit(url).path == self.path

    def handle(self, handler, url, body):
        handler.respond(200, b")]}'\n[[0,0,[\"CgtlocalVisitor\"]]]", {"Content-Type": "application/json"})

ENDPOINTS = (VisitorEndpoint(),)
