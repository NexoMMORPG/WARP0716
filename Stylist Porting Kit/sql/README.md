# SQL installation

Native Stylist by **ICE & SCIEL**.

Use your configured shared character/map database. Neither SQL file contains a database name, credentials or player data. Back up affected tables before importing.

| Situation | File |
| --- | --- |
| Fresh installation with no Stylist V2 tables | `install.sql` |
| Existing V2 item table missing `identity_verified` | `upgrade_existing_v2.sql`, once |
| Existing V2 tables already containing `identity_verified` | No schema update required |

For a fresh installation, run only `install.sql`. It creates these InnoDB tables:

- `native_stylist_character_v2`
- `native_stylist_item_v2`
- `native_stylist_look_v2`

The existing-V2 upgrade adds the item identity-verification column. It preserves stored recipe bytes. Existing item colors require their current item identity to be verified before automatic application.

`CREATE TABLE IF NOT EXISTS` does not upgrade an older table definition. Check the actual schema before selecting the upgrade. Do not run both files on a fresh installation and do not apply the one-time upgrade twice.

Both files create or update the Stylist tables. They do not create game accounts or grant items.
