# Server source patch

Native Stylist by **ICE & SCIEL**.

`native_stylist.patch` targets rAthena commit `d4b8e7b8f16061cc2496d377ac8f777ce72a4f39`. It modifies 14 existing source files and adds eight source files. It contains the client-version settings, packet and UI integration, character-selection definitions, appearance persistence, transaction-aware SQL support and matching hair catalogues.

The patch installs source code. Configure database connections in your normal rAthena configuration before generating the storage header.

## Apply

Back up your source checkout or retain a recoverable source revision. From the rAthena root, substitute the actual release path:

```text
git -c core.autocrlf=false apply --check "C:/path/to/Stylist Porting Kit/server/native_stylist.patch"
git -c core.autocrlf=false apply "C:/path/to/Stylist Porting Kit/server/native_stylist.patch"
```

The patch uses normal `a/src` and `b/src` paths. No custom path stripping is required. If the check fails, port the affected hunks to your revision before applying anything. Do not force mismatched hunks.

## Configure table names

After applying the patch, run:

```text
python "C:/path/to/Stylist Porting Kit/source/tools/configure_storage.py" --server C:/rathena --backup C:/backups/stylist-storage
```

The command reads the server's `conf/inter_athena.conf` import chain, validates identifiers and writes `src/custom/native_stylist_storage.hpp`. Use `--inter-config` for another root configuration file. Its backup directory must be fresh.

The included storage header is a deliberate compile-time placeholder. A successful configuration step replaces it with the target installation's table identifiers. The generator does not emit passwords or connection credentials.

The supported setup uses the same SQL host, port and database name for map and character storage. Different database names are rejected because appearance queries use the currently selected schema. Character and inventory table names can be customized.

## Schema and build

Choose the appropriate file from [sql](../sql/README.md), import it into your configured shared character/map database after a database backup, then compile rAthena using its normal build process.

The patch sets `PACKETVER 20250716` and disables packet obfuscation by default. Match the client packet settings to the server. To retain map packet encryption, remove the two packet-obfuscation undefines and configure the matching keys. Port these settings deliberately if you adapt the client profile. Do not include old or stock clients in testing the extension protocol.

Use the source patch or `source/tools/install_server.py` as the integration route. The installer route already generates the storage header but requires the documented version and obfuscation settings. Do not apply both routes to the same checkout.

## Rollback

Restore the per-install storage header placeholder from its configuration backup before checking patch reversal. Then, from the rAthena root:

```text
git -c core.autocrlf=false apply --reverse --check "C:/path/to/Stylist Porting Kit/server/native_stylist.patch"
git -c core.autocrlf=false apply --reverse "C:/path/to/Stylist Porting Kit/server/native_stylist.patch"
```

Retain the database backup and reverse source changes as part of the same maintenance plan. No destructive database rollback file is supplied.
