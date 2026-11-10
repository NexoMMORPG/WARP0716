# Native Stylist

Created by **ICE & SCIEL**.

Native character and costume recoloring for Ragnarok clients with a matching rAthena integration.

- Hair and skin
- Outfit materials, including supported seasonal outfits
- Equipped costume headgear and garments
- Eight saved Looks
- Live color picker, part highlights and mouse-wheel preview zoom
- Saved character and costume colors in character selection
- Confirm all saves the current editing session

## Start here

Read [INSTALL.md](docs/INSTALL.md) before applying the patch. Read [PORTING.md](docs/PORTING.md) before adapting it to another client or rAthena revision.

| Folder | Contents |
| --- | --- |
| `warp` | Selectable WARP QJS patch, patch-list entry and prebuilt build payload |
| `client` | One `native_stylist.grf` containing the prepared visual resources |
| `server` | Apply-ready rAthena source patch and server installation notes |
| `sql` | Fresh-install schema and the conditional existing-V2 upgrade |
| `source` | Matching client and server source, schema and build/install tools |
| `docs` | Installation, porting and resource guides |

The prebuilt patch supports Ragexe 2025-07-16 x86. Start with the clean client, select Native Stylist alongside your normal WARP choices, and apply them together. Hairstyle support is installed automatically. Native hook conflicts are checked before installation. Other client dates require porting.

The `native_stylist.dll` file is input to WARP. The patch embeds its contents into the executable. Players do not need a separate Stylist DLL beside their game executable, Python or Visual Studio.

Installation requires the matching game client and base data, WARP, rAthena source and a configured SQL database. The GRF supplements the existing game archives.

Source is provided under GPL-3.0-or-later. See [LICENSE](LICENSE) and [ASSETS.md](docs/ASSETS.md) for the distinction between source licensing and third-party game artwork.
