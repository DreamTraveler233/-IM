这是一个 wrk 基准测试示例集合。

用法示例：

1. 基本 GET 基准测试：
   bash scripts/bench/wrk_benchmark.sh http://127.0.0.1:8080

2. 单次直接运行：
   wrk -t4 -c200 -d30s --latency http://127.0.0.1:8080/api/v1/common/send-test

3. 使用 Lua 脚本发送 POST（示例）：
   wrk -t2 -c50 -d15s -s scripts/bench/wrk_post.lua --latency http://127.0.0.1:8080/api/v1/common/send-test

生成聚合报告（CSV/JSON/HTML/PNG）：

1. 创建 venv 并安装依赖（matplotlib）：
   ```bash
   bash scripts/bench/venv_setup.sh
   source scripts/bench/venv/bin/activate
   ```
2. 运行聚合脚本（将 bench_results/<timestamp> 替换为你的目录）：
   ```bash
   python3 scripts/bench/aggregate_wrk.py bench_results/20251212_174828
   ```
3. 在浏览器中查看报告（或本地预览）：
   ```bash
   bash scripts/bench/open_report.sh bench_results/20251212_174828
   # 在浏览器打开 http://127.0.0.1:8082/report.html
   ```

注意：
- 在进行大负载测试前确保服务器运行且监控资源（CPU、内存、磁盘、网络）。
- 产生大量并发时请注意客户端（运行 wrk 的机器）和服务器的文件描述符限制（`ulimit -n`）。
- 根据需要调整 `threads`、`connections` 和 `duration` 参数。
- 如果测试出现 Socket 错误（read/write），请观察系统日志或降低并发，或增加内核网络参数。
示例结果（你的环境）：
- `GET /_/status` (t=2,c=50,d=15s): ~230 req/s, 平均延迟 ~38ms。
- `GET /api/v1/common/send-test` (t=2,c=50,d=15s): ~1980 req/s, 平均延迟 <1ms（少量 socket 错误时）。
- `GET /media/test1m.bin` (t=4,c=200,d=20s): ~960 req/s, 带宽 ~0.94GB/s，平均延迟 ~185ms。

问题 & 建议：
- 如果出现大量 `Socket errors (read/write)`, 先检查客户端和服务器的文件描述符（`ulimit -n`），以及内核参数：`net.core.somaxconn`, `net.ipv4.tcp_tw_reuse` 等。
- 在高负载时监控 `top`, `ss -s`, `vmstat`, `iostat` 来定位瓶颈（CPU/内存/IO/网络/EPOLL backlog）。
- 对有数据库操作的 API（如发送消息）应单独基准测试，或使用更真实的场景（大并发、DAV、认证）。
- 若需要持续稳定大流量，请把 wrk 放在另一台机器上，避免客户端成为瓶颈。
