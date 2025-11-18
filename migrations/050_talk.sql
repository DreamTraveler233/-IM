  -- 050_talk.sql
-- 会话（talk）相关表：会话主表 / 用户会话视图 / 会话序列

-- 会话主实体：唯一标识一个会话（单聊=两人唯一，群聊=群唯一）
CREATE TABLE IF NOT EXISTS `im_talk` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '会话ID（由数据库自增）',
  `talk_mode` TINYINT NOT NULL COMMENT '会话类型: 1=单聊 2=群聊（对应 TalkModeEnum）',
  `user_min_id` BIGINT UNSIGNED NULL COMMENT '单聊: 两个用户ID中的较小者（用于唯一性约束）',
  `user_max_id` BIGINT UNSIGNED NULL COMMENT '单聊: 两个用户ID中的较大者（用于唯一性约束）',
  `group_id` BIGINT UNSIGNED NULL COMMENT '群聊: 群ID（用于唯一性约束）',
  `created_at` DATETIME NOT NULL COMMENT '创建时间',
  `updated_at` DATETIME NOT NULL COMMENT '更新时间',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_single` (`talk_mode`,`user_min_id`,`user_max_id`), -- 单聊唯一
  UNIQUE KEY `uk_group`  (`talk_mode`,`group_id`),                  -- 群聊唯一
  KEY `idx_mode` (`talk_mode`),
  CONSTRAINT `fk_talk_user_min` FOREIGN KEY (`user_min_id`) REFERENCES `im_user` (`id`) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT `fk_talk_user_max` FOREIGN KEY (`user_max_id`) REFERENCES `im_user` (`id`) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT `fk_talk_group` FOREIGN KEY (`group_id`) REFERENCES `im_group` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='会话主实体（单聊/群聊唯一）';

-- 用户的会话视图与设置：驱动左侧会话列表
CREATE TABLE IF NOT EXISTS `im_talk_session` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '主键ID',
  `user_id` BIGINT UNSIGNED NOT NULL COMMENT '会话归属用户ID（谁在看这个会话）',
  `talk_id` BIGINT UNSIGNED NOT NULL COMMENT '对应 im_talk.id',  -- 这里改为 BIGINT UNSIGNED
  `talk_mode` TINYINT NOT NULL COMMENT '会话类型: 1=单聊 2=群聊',
  `to_from_id` BIGINT UNSIGNED NOT NULL COMMENT '单聊: 对端用户ID；群聊: 群ID（用于 index_name 直达）',
  `is_disturb` TINYINT NOT NULL DEFAULT 2 COMMENT '免打扰: 1=开启 2=关闭',
  `is_top` TINYINT NOT NULL DEFAULT 2 COMMENT '置顶: 1=置顶 2=未置顶（前端最多18个）',
  `is_robot` TINYINT NOT NULL DEFAULT 2 COMMENT '是否机器人: 1=是 2=否',
  `unread_num` INT NOT NULL DEFAULT 0 COMMENT '未读消息数（本地缓存数，可通过 last_ack_seq 校准）',
  `last_ack_seq` BIGINT NOT NULL DEFAULT 0 COMMENT '上次確認已读的消息序号（会话内 seq）',
  `last_msg_id` CHAR(32) NULL COMMENT '最后一条消息ID（im_message.id，字符串）',
  `last_msg_type` SMALLINT NULL COMMENT '最后一条消息类型（对应 ChatMsgTypeXxx）',
  `last_msg_digest` VARCHAR(255) NULL COMMENT '最后一条消息预览文案（如[图片消息]、文本摘要）',
  `last_sender_id` BIGINT UNSIGNED NULL COMMENT '最后一条消息发送者ID',
  `draft_text` VARCHAR(1000) NULL COMMENT '草稿内容（富文本/纯文本皆可按需）',
  `name` VARCHAR(64) NULL COMMENT '名称快照（对端用户/群 名称）',
  `avatar` VARCHAR(255) NULL COMMENT '头像快照（对端用户/群 头像）',
  `remark` VARCHAR(64) NULL COMMENT '备注快照',
  `updated_at` DATETIME NOT NULL COMMENT '最后刷新时间（通常为最后消息时间）',
  `created_at` DATETIME NOT NULL COMMENT '创建时间',
  `deleted_at` DATETIME NULL COMMENT '软删除时间（用户从会话列表移除）',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_user_talk` (`user_id`,`talk_id`) COMMENT '每个用户对应的会话唯一',
  KEY `idx_user_updated` (`user_id`,`is_top`,`updated_at` DESC) COMMENT '会话列表排序：置顶优先、时间倒序',
  KEY `idx_user_unread` (`user_id`,`unread_num`) COMMENT '按未读筛选/统计',
  KEY `idx_user_indexname` (`user_id`,`talk_mode`,`to_from_id`) COMMENT 'index_name 定位：${talk_mode}_${to_from_id}',
  CONSTRAINT `fk_session_talk` FOREIGN KEY (`talk_id`) REFERENCES `im_talk` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_talk_session_user` FOREIGN KEY (`user_id`) REFERENCES `im_user` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_session_last_msg` FOREIGN KEY (`last_msg_id`) REFERENCES `im_message` (`id`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `fk_session_last_sender` FOREIGN KEY (`last_sender_id`) REFERENCES `im_user` (`id`) ON DELETE SET NULL ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户-会话视图/设置（置顶/免打扰/未读/草稿/最后消息）';

-- 会话序列表（可选）：维护 talk 的最新 sequence，便于并发安全自增
CREATE TABLE IF NOT EXISTS `im_talk_sequence` (
  `talk_id` BIGINT UNSIGNED NOT NULL COMMENT 'im_talk.id',
  `last_seq` BIGINT NOT NULL DEFAULT 0 COMMENT '当前会话内最大序号',
  `updated_at` DATETIME NOT NULL COMMENT '更新时间',
  `created_at` DATETIME NOT NULL COMMENT '创建时间',
  PRIMARY KEY (`talk_id`),
  CONSTRAINT `fk_seq_talk` FOREIGN KEY (`talk_id`) REFERENCES `im_talk` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='会话序列（分配消息 sequence 用）';
