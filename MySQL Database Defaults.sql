SET NAMES utf8;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
--  Table structure for `messages`
-- ----------------------------
DROP TABLE IF EXISTS `messages`;
CREATE TABLE `messages` (
  `rowid` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `target` text,
  `nick` text,
  `type` text,
  `message` longblob,
  `time` decimal(20,5) unsigned DEFAULT NULL,
  PRIMARY KEY (`rowid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- ----------------------------
--  Table structure for `settings`
-- ----------------------------
DROP TABLE IF EXISTS `settings`;
CREATE TABLE `settings` (
  `name` text,
  `value` text
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- ----------------------------
--  Records of `settings`
-- ----------------------------
BEGIN;
INSERT INTO `settings` VALUES ('replayAll', '0'), ('logLimit', '1'), ('logLevel', '1');
COMMIT;

-- ----------------------------
--  Table structure for `ignorelist`
-- ----------------------------
DROP TABLE IF EXISTS `ignorelist`;
CREATE TABLE `ignorelist` (
  `rowid` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `type` text,
  `target` text,
  PRIMARY KEY (`rowid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

SET FOREIGN_KEY_CHECKS = 1;
