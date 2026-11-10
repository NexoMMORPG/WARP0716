-- Native Stylist v2 appearance storage.
-- Definitions store the packed version 2 recipe with id zero. Native IDs are transient.
CREATE TABLE IF NOT EXISTS `native_stylist_character_v2` (
  `char_id` int unsigned NOT NULL,
  `definition` binary(156) NOT NULL,
  PRIMARY KEY (`char_id`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `native_stylist_item_v2` (
  `unique_id` bigint unsigned NOT NULL,
  `item_id` int unsigned NOT NULL,
  `definition` binary(156) NOT NULL,
  `identity_verified` tinyint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`unique_id`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `native_stylist_look_v2` (
  `char_id` int unsigned NOT NULL,
  `slot` tinyint unsigned NOT NULL,
  `hair` smallint unsigned NOT NULL,
  `definition` binary(156) NOT NULL,
  PRIMARY KEY (`char_id`,`slot`)
) ENGINE=InnoDB;
