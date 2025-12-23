#!/usr/bin/env python3

import argparse
import contextlib
import functools
import http.server
import socket
import socketserver


class NoCacheRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, directory=None, **kwargs):
        super().__init__(*args, directory=directory, **kwargs)

    def end_headers(self):
        # Avoid stale assets (Safari can be especially aggressive about caching)
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

    def do_GET(self):
        if self.path in ("", "/"):
            self.path = "/index.html"
        return super().do_GET()


def _find_free_port(host: str) -> int:
    with contextlib.closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.bind((host, 0))
        return s.getsockname()[1]


def main() -> None:
    parser = argparse.ArgumentParser(description="Serve the NAM Volume Knob web UI locally.")
    parser.add_argument("--host", default="127.0.0.1", help="Bind host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8000, help="Port to use (default: 8000; 0 = auto)")
    args = parser.parse_args()

    host = args.host
    port = args.port
    if port == 0:
        port = _find_free_port(host)

    # Always serve the web/ directory regardless of current working directory.
    web_dir = __file__.rsplit("/", 1)[0]
    handler = functools.partial(NoCacheRequestHandler, directory=web_dir)

    with socketserver.TCPServer((host, port), handler) as httpd:
        url = f"http://{host}:{port}/"
        print(f"Serving NAM Volume Knob web UI at: {url}")
        print("Press Ctrl+C to stop.")
        httpd.serve_forever()


if __name__ == "__main__":
    main()
