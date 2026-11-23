import json, pathlib, re, sys
root = pathlib.Path(sys.argv[1])
files = sorted((root / "themes").glob("*.json"))
assert len(files) >= 8, f"expected at least 8 themes, got {len(files)}"
ids = set()
required = {"background", "surface", "accent", "secondary", "tertiary", "text", "muted", "border", "error", "warning", "success"}
for file in files:
    data = json.loads(file.read_text())
    assert data.get("schemaVersion") == 1, file
    assert data["id"] not in ids, f"duplicate id: {data['id']}"
    ids.add(data["id"])
    assert data.get("name") and data.get("description"), file
    palette = data.get("palette", {})
    assert required <= palette.keys(), f"missing colors in {file}: {required-palette.keys()}"
    for key, color in palette.items():
        assert re.fullmatch(r"#[0-9A-Fa-f]{6}", color), f"invalid {key} in {file}"
    for key in ("wallpaper", "preview"):
        asset = (file.parent / data[key]).resolve()
        assert asset.is_file(), f"missing {key}: {asset}"
    assert len(data.get("terminal", {}).get("ansi", [])) == 8, file
print(f"Validated {len(files)} bundled themes, identifiers, palettes, and assets")
