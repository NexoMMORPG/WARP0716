# Porting

Native Stylist by **ICE & SCIEL**.

## Client profile

The shipped QJS supports Ragexe 2025-07-16 x86 and runs alongside other selected WARP patches on the clean client. Native host addresses remain specific to this client build. The image base is `0x00400000`, host ASLR is disabled, and a host SafeSEH table is unsupported by this profile.

The payload is a freestanding x86 PE without an entry point or imports. Internal HIGHLOW relocations are applied at WARP's actual allocation. The installer checks the native hook preimages in the current patch session and the matched payload checksum. It writes its new allocation and intended hooks while retaining existing patch data.

Native contracts are defined in `source/tools/client_profile_20250716.py`. Hairstyle support is included in `native_stylist_hair.qjs.in`, with its original WARP copyright and license notices. Existing compatible hairstyle adapters are followed through their native hook targets, so their allocated addresses can vary with the patch selection.

Important guarded sites include:

| Contract | Address |
| --- | --- |
| Packet type calls | `0xC147FE`, `0xC14824` |
| Packet reader | `0xC147D0` |
| Map dispatch | `0xC9E29C` |
| Window procedure operand | `0xDB669B` |
| Palette loader | `0x725B60` |
| Body and hair palette names | `0xB42580`, `0xB42DB0` |
| Costume head and garment paths | `0xB41D90`, `0xB442F0` |
| Sprite loader and palette converter | `0x7282D0`, `0x566770` |

Port every native contract used in `source/client`, including actor layouts, resource loading, input, rendering, palette construction, robe metadata and packet parsing. Updating the date or a few hook addresses alone is insufficient.

## Rebuild the WARP package

Requirements are Python 3.9 or later with `pefile`, Visual Studio x86 C++ tools and `vswhere`. Artwork preparation also requires Pillow. Python dependencies are listed in `source/requirements.txt`. Players applying the prebuilt patch do not need these build tools.

From `source`, use the clean reference client and fresh output and work directories:

```text
python tools/build_warp.py --client C:/inputs/Ragexe20250716.exe --out C:/build/warp --work C:/build/stylist-work
```

The builder checks the reference ABI, compiles with `/W4 /WX`, and emits the matched DLL, QJS and patch-list entry. The reference hash in its build manifest identifies the developer input. Users applying the patch can select their normal WARP choices in the same pass.

After applying WARP, verify the result without running the game:

```text
python tools/verify_warp_output.py --output C:/build/ClientWithStylist.exe --warp C:/build/warp
```

The verifier checks all installed Stylist hooks, hairstyle adapter limits and returns, module permissions, relocation, image bounds and PE settings. Other selected patches must also report success in WARP. A hook conflict requires resolving the affected integration rather than removing its guard.

WARP uses reflected reads to inspect staged changes. See the [official WARP Exe API](https://github.com/Neo-Mind/WARP/wiki/Exe-Object) and [Microsoft PE format reference](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).

## Protocol and server integration

The apply-ready server artifact is [server/native_stylist.patch](../server/native_stylist.patch). It targets rAthena commit `d4b8e7b8f16061cc2496d377ac8f777ce72a4f39`. Follow [the server guide](../server/README.md) to check the patch, apply it and generate the installation-specific storage header. SQL activation is a separate step described in [sql/README.md](../sql/README.md).

The supported database setup uses the same SQL host, port and database name for map and character storage. Both configuration tools enforce this condition. Table identifiers remain installation-specific.

Keep `source/client/protocol.hpp` identical across client, map server and character server.

| Packet | Bytes | Direction and purpose |
| --- | --- | --- |
| `0xD50` | 184 | Client request to map server |
| `0xD51` | 200 | Session state to client |
| `0xD52` | 164 | World appearance definition to client |
| `0xD53` | 332 | Equipped costume instances to client |
| `0xD54` | 164 | Character-selection definition to client |

These packet numbers must be free in the target setup. The map request registration and its packet database bound must match the supplied integration. Character-selection definitions are sent before the native character list. The client consumes them before mode-specific native parsing.

Preserve the map-loaded restoration hook, appearance refresh hooks, palette clamping, item-instance identity checks and character-selection integration. Updating only the command handler loses persistence or observer behavior.

Confirm all validates all edited item drafts before committing character and costume cosmetic rows in one InnoDB transaction. The transaction-aware common SQL adapter is required. The ordinary core save for base hairstyle and palette fields remains the server's separate asynchronous save.

Costume recoloring requires a persisted, owned, equipped instance with a valid unique identity. Unsupported identities fail closed. A visual preview does not prove an item can be durably saved.

## Resource compatibility

Material masks match exact source sprites and palette layouts. Changing a sprite without rebuilding its matching material can recolor the wrong pixels or make areas unavailable.

Head materials use NSM4 with a 1,424-byte layout. Body NSB files are 1,944 bytes and include their NSR1 palette remap. Do not mix these with older material formats.

Body material keys use the normalized exact native sprite resource path. Head material lookup follows the physical sprite and supports shared legacy palette names. Shared palette filenames do not imply shared masks. Mounted and ordinary forms may require separate profiles.

Human male, human female, Doram male and Doram female hairstyle catalogues must match the server's four arrays in `native_stylist_hairs.hpp`. Client native logical-to-physical hair mappings are build-specific.

UI artwork is external. Control positions, labels and interaction behavior are compiled in `window.cpp`. Replacing the GRF artwork does not change the window layout.

See [RESOURCES.md](RESOURCES.md) for archive contents and resource compatibility. Validate each new client profile with its matching server build.
