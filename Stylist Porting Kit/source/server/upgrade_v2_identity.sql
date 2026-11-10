-- One-time upgrade for an existing Native Stylist v2 item table.
-- Back up native_stylist_item_v2 before applying. Do not run on a fresh schema
-- that already has this column. Existing definition bytes remain unchanged.
-- Legacy colors stay available for explicit preview and confirmation, but are
-- not automatically applied until their current item identity is verified.
ALTER TABLE `native_stylist_item_v2`
  ADD COLUMN `identity_verified` tinyint unsigned NOT NULL DEFAULT 0 AFTER `definition`;
