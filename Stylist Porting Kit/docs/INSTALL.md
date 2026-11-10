# Installation

Native Stylist by **ICE & SCIEL**.

Install the matching client, resources and server integration together. Retain a recoverable copy of your executable, resource priority configuration, server sources and appearance tables.

## 1. Choose the clean client

Use the clean **2025-07-16 Ragexe x86** client supported by this release. File names do not determine compatibility. This profile checks the native functions and layouts it needs. Other client dates require porting.

Native Stylist is a normal selectable WARP patch. Select it with your usual patches and apply the complete selection in one pass.

Keep the normal client login, resource and packet settings matched to your server. Supply any files requested by your other selected patches. Native Stylist checks its own hook sites and reports a conflict if another modification makes a required hook incompatible.

## 2. Register and apply the WARP patch

Copy the release's `warp` contents into your WARP folder, preserving this structure:

```text
Scripts/Patches/NativeStylist.qjs
Patches/NativeStylist.yml
Inputs/NativeStylist/native_stylist.dll
Inputs/NativeStylist/Apply.yml
```

Append this entry to the existing `include` list in WARP's root `Patches.yml`:

```yaml
    - Patches/NativeStylist.yml
```

Keep the existing include entries. Restart WARP or reload its patch list. Select **Native Stylist by ICE & SCIEL** together with your other choices, load the clean client and save the patched executable to a new output file.

The patch supplies expanded hairstyle loading when needed and reuses a compatible hairstyle adapter already selected in the same session. When a separate hairstyle limit is selected, the chooser respects that limit. The shipped catalogues remain specific to the included resources. Shared head-palette choices are supported because recoloring masks follow the actual head sprite.

For console use, `Apply.yml` starts with Native Stylist alone. Edit its input and output paths, add your usual patch IDs and their saved inputs, then run from the WARP root:

```text
win32/WARP_console.exe -using Inputs/NativeStylist/Apply.yml
```

Create the output directory first. Check the patch result before using the executable. Some WARP versions write an output even after a patch error. Discard that output if any selected patch reports an error.

The DLL is a build input embedded into the executable. Players do not need a separate Stylist DLL beside their game executable.

## 3. Install the single GRF

Copy `client/native_stylist.grf` beside the patched executable. Add it first in your active GRF configuration. For a client using `DATA.INI`, the order is:

```ini
[Data]
0=native_stylist.grf
1=your_existing_custom.grf
2=data.grf
```

Retain your actual existing archives and their relative order. The filenames above are examples.

The new archive already contains the matching sprite corrections, palettes, material masks, hairstyle catalogues and current UI artwork. It replaces the need for multiple separate Stylist resource overlays. Do not also install older Stylist overlays. Existing base game archives remain required, including the original artwork for equipped costumes.

If your client reads loose data first, old loose Stylist files can override the new GRF. Move those old files into a separate backup before testing. Preserve unrelated loose game data.

## 4. Integrate rAthena

The source integration was developed against rAthena commit:

```text
d4b8e7b8f16061cc2496d377ac8f777ce72a4f39
```

It uses `PACKETVER 20250716`. Match the normal client login and packet-encryption settings to your server. The supplied installer requires exact source anchors and stops on a mismatch. Newer or customized checkouts may require manual porting.

The recommended community route is the source patch. From your rAthena source root, check and apply it:

```text
git -c core.autocrlf=false apply --check "C:/path/to/Stylist Porting Kit/server/native_stylist.patch"
git -c core.autocrlf=false apply "C:/path/to/Stylist Porting Kit/server/native_stylist.patch"
```

Use your actual release-folder path. The patch includes all 22 source files involved in the integration, including eight new files. It also sets `PACKETVER 20250716` and disables map packet obfuscation by default. Select the matching normal client settings. If you retain map packet encryption, remove the two packet-obfuscation undefines from `src/custom/defines_post.hpp` and configure matching client/server keys. Existing customized definitions may need manual merging.

Generate the installation-specific storage header before building:

```text
python "C:/path/to/Stylist Porting Kit/source/tools/configure_storage.py" --server C:/rathena --backup C:/backups/stylist-storage
```

The patch deliberately installs a compile-time placeholder for this header. The command replaces it using your actual database and table names. No credentials are written to the header. See [server/README.md](../server/README.md) for the complete patch procedure.

As an alternative to applying the patch, the existing source installer can perform the integration. From the release's `source` directory, run with Python 3.9 or later:

```text
python tools/install_server.py --server C:/rathena --backup C:/backups/stylist-server
```

Choose one integration route. Do not apply the source patch on top of the installer or the installer on top of the source patch. Use a fresh backup directory. Inspect the generated manifest before compiling. The installer reads your server's normal `conf/inter_athena.conf` import chain. Use `--inter-config` if you use another root configuration file.

The installer route generates `src/custom/native_stylist_storage.hpp` automatically. For that route, also set `PACKETVER 20250716` in `src/custom/defines_pre.hpp` and undefine `PACKET_OBFUSCATION` and `PACKET_OBFUSCATION_WARN` in `src/custom/defines_post.hpp` before compiling.

Map and character storage must use the same SQL endpoint and the same database name. The current appearance queries are relative to each connection's selected database. Both configuration tools reject different database names, rather than creating independent sets of appearance tables.

## 5. Activate appearance storage

Back up the affected database before activation. For a fresh installation, import:

```text
sql/install.sql
```

Import it into the character database selected by your inter-server configuration. It creates the character, item and saved-Look tables. Keep these tables as InnoDB.

For an existing V2 installation whose item table does not have `identity_verified`, back up that table and apply `sql/upgrade_existing_v2.sql` once. A fresh schema already contains that column. Do not apply the upgrade twice. See [sql/README.md](../sql/README.md) to choose the right file.

Compile your server with its normal build process, then install the matched binaries during your maintenance window. This package does not start, stop or configure a server automatically.

## 6. First use

Open the window with `@nativestylist` using an account whose command permissions allow it. A server operator can expose the same entry point through an NPC or a chosen permission group.

Preview hair, skin, outfit and costume edits. Confirm all applies the current session's drafts. Saved Looks contain character hairstyle and character colors. Costume colors remain attached to their individual item instances.

All observing players need the matched client patch and resources. Stock clients cannot interpret the extension packets and transient appearance identifiers.

## Rollback

To roll back, restore your matched executable, resource priority configuration, server sources and binaries. Keep appearance-table backups until rollback is accepted. Removing the GRF alone does not remove the server protocol integration.
