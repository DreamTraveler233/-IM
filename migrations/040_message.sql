-- 040_message.sql
-- 消息相关表（消息主表/转发/已读/删除/提及），使用字符串消息ID（CHAR(32)）

-- 消息主表
CREATE TABLE IF NOT EXISTS `im_message` (
  `id` CHAR(32) NOT NULL COMMENT '消息ID（UUID/HEX32，无自增）',
  `talk_id` BIGINT UNSIGNED NOT NULL COMMENT '所属会话ID（im_talk.id）',
  `sequence` BIGINT NOT NULL COMMENT '会话内消息序号（严格递增，用于排序/分页）',
  `talk_mode` TINYINT NOT NULL COMMENT '会话类型: 1=单聊 2=群聊',
  `msg_type` SMALLINT NOT NULL COMMENT '消息类型（文本/图片/音视频/文件/位置/名片/转发/登录/投票/混合/公告/系统）',
  `sender_id` BIGINT UNSIGNED NOT NULL COMMENT '发送者用户ID',
  `receiver_id` BIGINT UNSIGNED NULL COMMENT '单聊: 对端用户ID；群聊: NULL',
  `group_id` BIGINT UNSIGNED NULL COMMENT '群聊: 群ID；单聊: NULL',
  `content_text` MEDIUMTEXT NULL COMMENT '文本/系统文本内容；非文本消息留空（由 extra 承载）',
  `extra` JSON NULL COMMENT '扩展内容：各类型专属字段',
  `quote_msg_id` CHAR(32) NULL COMMENT '被引用消息ID（用于回复/引用）',
  `is_revoked` TINYINT NOT NULL DEFAULT 2 COMMENT '撤回状态: 1=已撤回 2=正常',
  `revoke_by` BIGINT UNSIGNED NULL COMMENT '撤回操作人用户ID（可为空）',
  `revoke_time` DATETIME NULL COMMENT '撤回时间',
  `created_at` DATETIME NOT NULL COMMENT '发送时间（创建时间）',
  `updated_at` DATETIME NOT NULL COMMENT '更新时间',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_talk_seq` (`talk_id`,`sequence`) COMMENT '同一会话序号唯一',
  KEY `idx_talk_seq_desc` (`talk_id`,`sequence` DESC) COMMENT '按序号倒序分页',
  KEY `idx_sender_time` (`sender_id`,`created_at`) COMMENT '按发送者与时间检索',
  KEY `idx_receiver_time` (`receiver_id`,`created_at`) COMMENT '按单聊接收者与时间检索',
  KEY `idx_group_time` (`group_id`,`created_at`) COMMENT '按群与时间检索',
  KEY `idx_quote` (`quote_msg_id`) COMMENT '引用消息反查',
  CONSTRAINT `fk_msg_talk` FOREIGN KEY (`talk_id`) REFERENCES `im_talk` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_msg_sender` FOREIGN KEY (`sender_id`) REFERENCES `im_user` (`id`) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT `fk_msg_receiver` FOREIGN KEY (`receiver_id`) REFERENCES `im_user` (`id`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `fk_msg_group` FOREIGN KEY (`group_id`) REFERENCES `im_group` (`id`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `fk_msg_quote` FOREIGN KEY (`quote_msg_id`) REFERENCES `im_message` (`id`) ON DELETE SET NULL ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='消息主表（字符串ID）';

-- 消息转发映射：一条“转发消息” -> 多条“来源消息”
CREATE TABLE IF NOT EXISTS `im_message_forward_map` (
  `forward_msg_id` CHAR(32) NOT NULL COMMENT '转发生成的新消息ID（im_message.id）',
  `src_msg_id` CHAR(32) NOT NULL COMMENT '被转发的原消息ID（im_message.id）',
  `src_talk_id` BIGINT UNSIGNED NOT NULL COMMENT '原消息所属会话ID（im_talk.id）',
  `src_sender_id` BIGINT UNSIGNED NOT NULL COMMENT '原消息发送者用户ID',
  `created_at` DATETIME NOT NULL COMMENT '创建时间',
  PRIMARY KEY (`forward_msg_id`,`src_msg_id`),
  KEY `idx_src` (`src_msg_id`),
  CONSTRAINT `fk_forward_msg` FOREIGN KEY (`forward_msg_id`) REFERENCES `im_message` (`id`) ON UPDATE CASCADE ON DELETE CASCADE,
  CONSTRAINT `fk_forward_src_msg` FOREIGN KEY (`src_msg_id`) REFERENCES `im_message` (`id`) ON UPDATE CASCADE ON DELETE CASCADE,
  CONSTRAINT `fk_forward_src_talk` FOREIGN KEY (`src_talk_id`) REFERENCES `im_talk` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='消息转发映射（字符串ID）';

-- 消息已读：记录某用户对某条消息的阅读
CREATE TABLE IF NOT EXISTS `im_message_read` (
  `msg_id` CHAR(32) NOT NULL COMMENT '消息ID（im_message.id）',
  `user_id` BIGINT UNSIGNED NOT NULL COMMENT '阅读用户ID',
  `read_at` DATETIME NOT NULL COMMENT '阅读时间',
  PRIMARY KEY (`msg_id`,`user_id`),
  KEY `idx_user_time` (`user_id`,`read_at`),
  CONSTRAINT `fk_read_msg` FOREIGN KEY (`msg_id`) REFERENCES `im_message` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_read_user` FOREIGN KEY (`user_id`) REFERENCES `im_user` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='消息已读表（字符串ID）';

-- 用户删除消息（本地删除，不影响对端）
CREATE TABLE IF NOT EXISTS `im_message_user_delete` (
  `msg_id` CHAR(32) NOT NULL COMMENT '消息ID（im_message.id）',
  `user_id` BIGINT UNSIGNED NOT NULL COMMENT '删除动作的用户ID',
  `deleted_at` DATETIME NOT NULL COMMENT '删除时间',
  PRIMARY KEY (`msg_id`,`user_id`),
  KEY `idx_user_deleted` (`user_id`,`deleted_at`),
  CONSTRAINT `fk_delete_msg` FOREIGN KEY (`msg_id`) REFERENCES `im_message` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_delete_user` FOREIGN KEY (`user_id`) REFERENCES `im_user` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户侧消息删除记录（字符串ID）';

-- @ 提及（可选）：支持“被@”快速查询
CREATE TABLE IF NOT EXISTS `im_message_mention` (
  `msg_id` CHAR(32) NOT NULL COMMENT '消息ID（im_message.id）',
  `mentioned_user_id` BIGINT UNSIGNED NOT NULL COMMENT '@到的用户ID',
  PRIMARY KEY (`msg_id`,`mentioned_user_id`),
  KEY `idx_mentioned` (`mentioned_user_id`,`msg_id`),
  CONSTRAINT `fk_mention_msg` FOREIGN KEY (`msg_id`) REFERENCES `im_message` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_mention_user` FOREIGN KEY (`mentioned_user_id`) REFERENCES `im_user` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='消息@提及映射（字符串ID）';
