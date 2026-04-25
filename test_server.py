import sys
from http.server import HTTPServer, BaseHTTPRequestHandler

port = int(sys.argv[1])

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.end_headers()
        self.wfile.write(f"Hello from backend on port {port}!\n".encode())

print(f"Starting test server on port {port}...")
HTTPServer(("", port), Handler).serve_forever()