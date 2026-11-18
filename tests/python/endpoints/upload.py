from urllib.parse import parse_qs, urlsplit

# https://github.com/SimoHypers/limusic/blob/f82fc1ec0fe1e12ea023cb6da1550a911aa80eaf/crates/innertube/src/endpoints.rs#L1267
class PlaylistCoverUploadEndpoint:
    method = "POST"
    path = "/playlist_image_upload/playlist_custom_thumbnail"

    def matches(self, method, url):
        return method == self.method and urlsplit(url).path == self.path

    def handle(self, handler, url, body):
        command = handler.headers.get("X-Goog-Upload-Command")
        if command == "start":
            valid = not body and handler.headers.get("X-Goog-Upload-Protocol") == "resumable"
            valid = valid and handler.headers.get("X-Goog-Upload-Header-Content-Length") == "11"
            if valid:
                handler.respond(200, headers={"x-guploader-uploadid": "local-upload"})
                return
        elif command == "upload, finalize":
            query = parse_qs(urlsplit(url).query)
            valid = body == b"local-image" and query.get("upload_id") == ["local-upload"]
            valid = valid and query.get("upload_protocol") == ["resumable"]
            valid = valid and handler.headers.get("X-Goog-Upload-Offset") == "0"
            if valid:
                handler.respond(
                    200,
                    b'{"playlistScottyEncryptedBlobId":"local-blob"}',
                    {"Content-Type": "application/json"},
                )
                return
        handler.send_error(400)

ENDPOINTS = (PlaylistCoverUploadEndpoint(),)
