"""Serve ota_out/ over HTTP for pull-OTA testing, optionally throttled.

Run from BuddyBotFirmware/:

    python tools/serve_ota.py                 # same as python -m http.server 8000 --directory ota_out
    python tools/serve_ota.py --kbps 20       # slow download, ~1 min per MB: time to Ctrl+C mid-transfer
"""
import argparse
import functools
import time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


class ThrottledHandler(SimpleHTTPRequestHandler):
    bytes_per_sec = 0  # 0 = unthrottled

    def copyfile(self, source, outputfile):
        if not self.bytes_per_sec:
            return super().copyfile(source, outputfile)
        chunk = max(1024, self.bytes_per_sec // 10)
        while data := source.read(chunk):
            outputfile.write(data)
            time.sleep(len(data) / self.bytes_per_sec)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--dir", default="ota_out", help="folder to serve (relative to BuddyBotFirmware/)")
    parser.add_argument("--kbps", type=int, default=0, help="throttle downloads to this many KB/s (0 = off)")
    args = parser.parse_args()

    ThrottledHandler.bytes_per_sec = args.kbps * 1024
    handler = functools.partial(ThrottledHandler, directory=str(ROOT / args.dir))
    throttle = f", throttled to {args.kbps} KB/s" if args.kbps else ""
    print(f"Serving {ROOT / args.dir} on port {args.port}{throttle}. Ctrl+C to stop.")
    ThreadingHTTPServer(("", args.port), handler).serve_forever()


if __name__ == "__main__":
    main()
