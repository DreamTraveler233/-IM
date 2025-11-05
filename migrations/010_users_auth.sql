-- 用户与认证
-- 此迁移文件创建用户认证相关的数据库表，包括用户基本信息、设置、令牌、OAuth账户、登录日志、验证码等。

-- 用户表：存储用户基本信息和认证数据
CREATE TABLE IF NOT EXISTS users (
  id                BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,  -- 用户唯一ID，自增主键
  mobile            VARCHAR(20) NOT NULL,                         -- 手机号，唯一，用于登录
  email             VARCHAR(128),                                 -- 邮箱，可选，唯一，用于找回密码
  nickname          VARCHAR(64) NOT NULL,                         -- 昵称，显示名称
  password_hash     VARCHAR(255) NOT NULL,                        -- 密码哈希，使用PBKDF2+盐
  avatar            VARCHAR(255),                                 -- 头像URL，可选
  motto             VARCHAR(255),                                 -- 个性签名，可选
  birthday          DATE,                                        -- 生日，可选
  gender            TINYINT UNSIGNED NOT NULL DEFAULT 0,         -- 性别：0- 1男 2女 3未知
  is_robot          TINYINT UNSIGNED NOT NULL DEFAULT 0,         -- 是否机器人：0否 1是
  is_qiye           TINYINT UNSIGNED NOT NULL DEFAULT 0,         -- 是否企业用户：0否 1是
  status            TINYINT UNSIGNED NOT NULL DEFAULT 1,         -- 账户状态：1正常 2禁用
  last_login_at     DATETIME,                                     -- 最后登录时间
  created_at        DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  updated_at        DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  UNIQUE KEY uq_users_mobile (mobile),                           -- 手机号唯一索引
  UNIQUE KEY uq_users_email (email)                              -- 邮箱唯一索引
) ENGINE=InnoDB;

-- 用户设置：存储用户的个性化设置，如主题、通知等
CREATE TABLE IF NOT EXISTS user_settings (
  user_id              BIGINT UNSIGNED PRIMARY KEY,               -- 用户ID，外键关联users.id
  theme_mode           VARCHAR(16) NOT NULL DEFAULT 'light',       -- 主题模式：light亮色 dark暗色
  theme_bag_img        VARCHAR(255),                               -- 主题背景图片URL
  theme_color          VARCHAR(16) NOT NULL DEFAULT '#409eff',     -- 主题颜色，十六进制
  notify_cue_tone      VARCHAR(64) NOT NULL DEFAULT 'default',     -- 通知提示音
  keyboard_event_notify VARCHAR(8) NOT NULL DEFAULT 'N',           -- 键盘事件通知：Y是 N否
  created_at           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 创建时间
  updated_at           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  CONSTRAINT fk_user_settings_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE -- 外键约束，级联删除
) ENGINE=InnoDB;

-- 用户OAuth账户：存储第三方登录信息，如微信、QQ等
CREATE TABLE IF NOT EXISTS user_oauth_accounts (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,       -- OAuth记录ID
  user_id       BIGINT UNSIGNED NOT NULL,                         -- 用户ID
  provider      VARCHAR(32) NOT NULL,                              -- 提供商：wechat, qq等
  open_id       VARCHAR(128) NOT NULL,                             -- 开放ID，第三方唯一标识
  union_id      VARCHAR(128),                                      -- 联合ID，可选，用于跨应用
  access_token  VARCHAR(255),                                      -- 访问令牌
  refresh_token VARCHAR(255),                                      -- 刷新令牌
  expires_at    DATETIME,                                          -- 令牌过期时间
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,      -- 创建时间
  updated_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
  UNIQUE KEY uq_user_oauth (provider, open_id),                   -- 提供商+开放ID唯一
  KEY idx_user_oauth_user (user_id, provider),                    -- 用户+提供商索引
  CONSTRAINT fk_user_oauth_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE -- 外键约束
) ENGINE=InnoDB;

-- 用户登录日志：记录用户登录历史，用于安全审计
CREATE TABLE IF NOT EXISTS user_login_logs (
  id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,        -- 日志ID
  user_id      BIGINT UNSIGNED NOT NULL,                          -- 用户ID
  platform     VARCHAR(32) NOT NULL,                               -- 登录平台
  login_at     DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,       -- 登录时间
  status       TINYINT UNSIGNED NOT NULL DEFAULT 1,                -- 登录状态：1成功 2失败
  KEY idx_user_login_logs_user (user_id, login_at),               -- 用户+时间索引
  CONSTRAINT fk_user_login_logs_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE -- 外键约束
) ENGINE=InnoDB;

-- 短信验证码：存储短信验证码，用于手机号验证
CREATE TABLE IF NOT EXISTS sms_verification_codes (
  id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,        -- 验证码ID
  mobile       VARCHAR(20) NOT NULL,                              -- 手机号
  code         VARCHAR(10) NOT NULL,                               -- 验证码内容
  channel      VARCHAR(32) NOT NULL,                               -- 发送渠道：aliyun, tencent等
  status       TINYINT UNSIGNED NOT NULL DEFAULT 0,               -- 状态：0待验证 1成功 2失效
  expired_at   DATETIME NOT NULL,                                 -- 过期时间
  used_at      DATETIME,                                          -- 使用时间
  send_ip      VARCHAR(45),                                       -- 发送请求IP
  created_at   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,      -- 创建时间
  KEY idx_sms_mobile_channel (mobile, channel, status),          -- 手机号+渠道+状态索引
  KEY idx_sms_expired_at (expired_at)                             -- 过期时间索引，便于清理
) ENGINE=InnoDB;

-- 邮箱验证码：存储邮箱验证码，用于邮箱验证
CREATE TABLE IF NOT EXISTS email_verification_codes (
  id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,        -- 验证码ID
  email        VARCHAR(128) NOT NULL,                             -- 邮箱地址
  code         VARCHAR(10) NOT NULL,                               -- 验证码内容
  channel      VARCHAR(32) NOT NULL,                               -- 发送渠道：smtp, sendgrid等
  status       TINYINT UNSIGNED NOT NULL DEFAULT 0,               -- 状态：0待验证 1成功 2失效
  expired_at   DATETIME NOT NULL,                                 -- 过期时间
  used_at      DATETIME,                                          -- 使用时间
  send_ip      VARCHAR(45),                                       -- 发送请求IP
  created_at   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,      -- 创建时间
  KEY idx_email_channel (email, channel, status),                -- 邮箱+渠道+状态索引
  KEY idx_email_expired_at (expired_at)                           -- 过期时间索引
) ENGINE=InnoDB;

-- 存储文件：管理用户上传的文件，如头像、附件等
CREATE TABLE IF NOT EXISTS storage_files (
  id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,       -- 文件ID
  user_id       BIGINT UNSIGNED,                                  -- 上传用户ID，可为空（匿名上传）
  drive         TINYINT UNSIGNED NOT NULL DEFAULT 1,              -- 存储驱动：1本地 2OSS等
  file_name     VARCHAR(255) NOT NULL,                            -- 原始文件名
  file_ext      VARCHAR(32),                                      -- 文件扩展名
  mime_type     VARCHAR(128),                                     -- MIME类型
  file_size     BIGINT UNSIGNED NOT NULL DEFAULT 0,               -- 文件大小（字节）
  file_path     VARCHAR(512) NOT NULL,                            -- 文件存储路径
  md5_hash      CHAR(32),                                         -- MD5哈希，用于去重
  sha1_hash     CHAR(40),                                         -- SHA1哈希，备用
  status        TINYINT UNSIGNED NOT NULL DEFAULT 1,              -- 文件状态：1正常 2删除
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,     -- 创建时间
  deleted_at    DATETIME,                                         -- 删除时间（软删除）
  KEY idx_storage_user (user_id, status),                        -- 用户+状态索引
  KEY idx_storage_hash (md5_hash),                               -- 哈希索引，便于查找重复文件
  CONSTRAINT fk_storage_files_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE SET NULL -- 外键约束，删除用户时置空
) ENGINE=InnoDB;
