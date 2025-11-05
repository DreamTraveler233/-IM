
-- 联系人分组表：存储每个用户的联系人分组（如“朋友”、“同事”等）
CREATE TABLE IF NOT EXISTS contact_groups (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 分组ID，自增主键
  user_id       BIGINT UNSIGNED NOT NULL,                   -- 所属用户ID
  name          VARCHAR(32) NOT NULL,                       -- 分组名称
  sort          INT UNSIGNED NOT NULL DEFAULT 100,          -- 分组排序值，越小越靠前
  contact_count INT UNSIGNED NOT NULL DEFAULT 0,            -- 分组下联系人数量
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,-- 创建时间
  updated_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  UNIQUE KEY uq_contact_group_user_name (user_id, name),    -- 用户+分组名唯一
  KEY idx_contact_group_sort (user_id, sort),               -- 用户+排序索引
  CONSTRAINT fk_contact_groups_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE -- 外键约束，级联删除
) ENGINE=InnoDB;


-- 联系人表：存储每个用户的联系人（好友/黑名单等）
CREATE TABLE IF NOT EXISTS contacts (
  id             BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 记录ID，自增主键
  user_id        BIGINT UNSIGNED NOT NULL,                   -- 用户ID
  contact_id     BIGINT UNSIGNED NOT NULL,                   -- 联系人用户ID
  group_id       BIGINT UNSIGNED,                            -- 所属分组ID，可为空
  remark         VARCHAR(64),                                -- 备注名
  relation       TINYINT UNSIGNED NOT NULL DEFAULT 1,        -- 关系类型：1陌生人 2好友 3企业同事 4本人
  status         TINYINT UNSIGNED NOT NULL DEFAULT 1,        -- 状态：1正常 2删除
  created_at     DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,-- 创建时间
  updated_at     DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  UNIQUE KEY uq_contact_pair (user_id, contact_id),          -- 用户+联系人唯一
  KEY idx_contacts_group (user_id, group_id),                -- 用户+分组索引
  CONSTRAINT fk_contacts_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE, -- 外键约束
  CONSTRAINT fk_contacts_contact FOREIGN KEY (contact_id) REFERENCES users (id) ON DELETE CASCADE, -- 外键约束
  CONSTRAINT fk_contacts_group FOREIGN KEY (group_id) REFERENCES contact_groups (id) ON DELETE SET NULL -- 外键约束
) ENGINE=InnoDB;


-- 联系人申请表：记录加好友/加黑名单等申请流程
CREATE TABLE IF NOT EXISTS contact_applies (
  id             BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 申请ID，自增主键
  applicant_id   BIGINT UNSIGNED NOT NULL,                   -- 申请人用户ID
  target_id      BIGINT UNSIGNED NOT NULL,                   -- 目标用户ID
  remark         VARCHAR(255),                               -- 申请备注
  status         TINYINT UNSIGNED NOT NULL DEFAULT 0,        -- 申请状态：0待处理 1同意 2拒绝
  handle_remark  VARCHAR(255),                               -- 处理备注
  handled_at     DATETIME,                                   -- 处理时间
  created_at     DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,-- 创建时间
  updated_at     DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  KEY idx_contact_apply_status (target_id, status, created_at), -- 目标+状态+时间索引
  KEY idx_contact_apply_applicant (applicant_id, created_at),   -- 申请人+时间索引
  CONSTRAINT fk_contact_apply_applicant FOREIGN KEY (applicant_id) REFERENCES users (id) ON DELETE CASCADE, -- 外键约束
  CONSTRAINT fk_contact_apply_target FOREIGN KEY (target_id) REFERENCES users (id) ON DELETE CASCADE,       -- 外键约束
  CONSTRAINT fk_contact_apply_handler FOREIGN KEY (handler_id) REFERENCES users (id) ON DELETE SET NULL     -- 外键约束
) ENGINE=InnoDB;
