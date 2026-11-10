# Resources

The release installs one archive, `native_stylist.grf`. Place it before existing game archives in the client's active GRF list.

| Archive path | Purpose |
| --- | --- |
| `data/texture/native_stylist` | Window artwork, buttons, hover and selected states, color grid, picker and swatches |
| `data/native_stylist/materials` | Hair and skin NSM4 materials |
| `data/native_stylist/body_materials` | Body materials and palette remaps |
| `data/native_stylist/palettes` | Source-specific base palettes |
| `data/native_stylist/hairs_*.bin` | Four sex and species hairstyle catalogues |
| `data/sprite` | Matching character, hair and seasonal-outfit sprite and animation overlays |
| `data/palette` | Matching native legacy palette resources |

Optional custom costume grouping files use `data/native_stylist/costumes/*.ncm`. The current archive does not require them. The client derives default costume color areas from the equipped costume's existing game sprite.

This archive contains patch resources. It does not include all costumes, item tables, maps or the rest of the base game. Keep the existing game archives.

[BUILD.json](BUILD.json) records the archive contents, sizes and SHA256.

## Rebuild from matching prepared inputs

From `source`:

```text
python tools/build_resource_grf.py --data C:/prepared/data --grf C:/prepared/highest.grf --grf C:/prepared/lower.grf --out C:/build/native_stylist.grf --report C:/build/resources.json
```

Supply existing visual overlays in priority order, highest first. The prepared loose data wins over those archives. The tool reads `native_stylist/materials.json` as a build input and includes its matching active material files.

Every packed entry is decompressed and checked against its winning source hash. Sprite masks and catalogues must still be generated and reviewed for the chosen assets. See [PORTING.md](PORTING.md) for compatibility requirements.

## Prepare materials and artwork

These developer tools require Python 3.9 or later and the dependencies in `source/requirements.txt`. The corrected input folder must contain the matching `data/sprite` tree. The supplied profiles enforce exact sprite fingerprints, so different artwork needs matching profiles before generation.

From `source`, prepare current materials, then picker artwork, then the classic theme:

```text
python tools/prepare_resources.py --v2 --grf C:/assets/base.grf --corrected C:/assets/corrected --out C:/prepared/data
python tools/prepare_picker_art.py --source C:/prepared/data/texture/native_stylist --out C:/prepared/data/texture/native_stylist
python tools/prepare_classic_theme.py --grf C:/assets/base.grf --texture C:/prepared/data/texture/native_stylist --out C:/prepared/data/texture/native_stylist
```

Run the classic theme generator last. It includes the color-area controls. Keep all generated artwork in the prepared data tree used by the GRF builder.

When changing material labels, pass `--labels-header` to `prepare_resources.py` and use that generated header as `source/client/material_labels.hpp` before rebuilding the payload. When changing hairstyles, run `prepare_hair_catalog_v2.py` with matching sprite pairs and native tables, then use its generated catalogues and server header together. The prebuilt GRF already contains the matching resources for this release.
