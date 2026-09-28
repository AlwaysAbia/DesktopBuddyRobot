"""Package the last build for pull OTA: copies firmware.bin and writes manifest.json.

Run from BuddyBotFirmware/ after `pio run -e esp32dev`:

    python tools/make_ota_manifest.py --base-url http://192.168.1.100:8000

Output goes to ota_out/ (gitignored). Serve it locally with:

    python -m http.server 8000 --directory ota_out

or upload both files somewhere public and pass that folder's URL as --base-url.
"""
import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def firmware_version() -> str:
    text = (ROOT / "include" / "ota_config.h").read_text(encoding="utf-8")
    match = re.search(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"', text)
    if not match:
        raise SystemExit("FIRMWARE_VERSION not found in include/ota_config.h")
    return match.group(1)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base-url", required=True, help="URL of the folder the files will be served from")
    parser.add_argument("--env", default="esp32dev", help="PlatformIO environment that was built")
    parser.add_argument("--out", default="ota_out", help="output folder (relative to BuddyBotFirmware/)")
    args = parser.parse_args()

    source = ROOT / ".pio" / "build" / args.env / "firmware.bin"
    if not source.exists():
        raise SystemExit(f"{source} not found - run `pio run -e {args.env}` first")

    version = firmware_version()
    out_dir = ROOT / args.out
    out_dir.mkdir(exist_ok=True)

    bin_name = f"firmware-{version}.bin"
    shutil.copyfile(source, out_dir / bin_name)
    md5 = hashlib.md5((out_dir / bin_name).read_bytes()).hexdigest()

    manifest = {
        "version": version,
        "url": f"{args.base_url.rstrip('/')}/{bin_name}",
        "md5": md5,
    }
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    print(f"Wrote {out_dir / bin_name} ({(out_dir / bin_name).stat().st_size} bytes)")
    print(f"Wrote {out_dir / 'manifest.json'}:")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
