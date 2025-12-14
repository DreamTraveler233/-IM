#!/usr/bin/env python3
"""
Parse wrk output files and generate CSV/JSON/PNG/HTML report.
Usage: scripts/bench/aggregate_wrk.py <bench_dir>
Example: scripts/bench/aggregate_wrk.py bench_results/20251212_174828
"""
import sys
import re
import os
import glob
import json
import csv
from collections import defaultdict

import matplotlib
matplotlib.use('Agg')
# Prefer a CJK-capable font if available to avoid glyph missing warnings
try:
    import matplotlib.font_manager as fm
    # common CJK font names to try
    cjk_fonts = [
        'Noto Sans CJK SC',
        'Noto Sans CJK JP',
        'Noto Sans CJK KR',
        'WenQuanYi Micro Hei',
        'SimHei',
        'Source Han Sans CN',
    ]
    available = [f.name for f in fm.fontManager.ttflist]
    for f in cjk_fonts:
        if f in available:
            matplotlib.rcParams['font.sans-serif'] = [f]
            break
    # Ensure minus sign renders
    matplotlib.rcParams['axes.unicode_minus'] = False
except Exception:
    pass
import matplotlib.pyplot as plt

# Utility functions
re_running = re.compile(r"Running\s+([\d\.]+[smh]) test @\s+(.+)")
re_threads_conns = re.compile(r"(\d+) threads? and (\d+) connections")
re_latency_line = re.compile(r"Latency\s+([\d\.a-zA-Z/]+)\s+([\d\.a-zA-Z/]+)\s+([\d\.a-zA-Z/]+)")
re_req_sec_line = re.compile(r"Requests/sec:\s*([\d\.]+)")
re_transfer_sec_line = re.compile(r"Transfer/sec:\s*([\d\.]+)\s*([KMG]B|B)?")
re_total_reqs_line = re.compile(r"\s*([0-9,]+) requests in")
re_latency_dist = re.compile(r"^\s*(\d+)%\s+([\d\.]+(?:ms|us|s)?)", re.MULTILINE)
re_socket_errors = re.compile(r"Socket errors:\s*(.*)")
re_threadstats = re.compile(r"Thread Stats\s+Avg\s+Stdev\s+Max")

unit_map = {'B':1, 'KB':1024, 'MB':1024**2, 'GB':1024**3}


def to_bytes(val, unit):
    if not unit:
        # some wrk version doesn't add unit; try to infer
        return float(val)
    return float(val) * unit_map.get(unit.upper(), 1)


def parse_latency(val):
    # wrk avg latency expresses in ms or us or s, e.g. 115.45ms or 818.96us or 1.2s
    if val.endswith('ms'):
        return float(val[:-2]) / 1000.0
    if val.endswith('us'):
        return float(val[:-2]) / 1_000_000.0
    if val.endswith('s'):
        return float(val[:-1])
    # fallback assume ms
    return float(val) / 1000.0


def parse_file(path):
    data = defaultdict(lambda: None)
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        text = f.read()
    if 'unable to connect' in text.lower() or 'connection refused' in text.lower():
        data['error'] = 'connection_error'
        data['raw'] = text
        return data

    # running/duration
    m = re_running.search(text)
    if m:
        data['duration'] = m.group(1)
        data['url'] = m.group(2).replace('\n', '').strip()

    # threads/conns
    m = re_threads_conns.search(text)
    if m:
        data['threads'] = int(m.group(1))
        data['conns'] = int(m.group(2))

    # latency average/stdev/max
    # find the Thread Stats section and then parse the first relevant line with 'Latency'
    m = re_latency_line.search(text)
    if m:
        data['lat_avg'] = parse_latency(m.group(1))
        # leave stdev as string, parse if has ms/us
        data['lat_stdev'] = parse_latency(m.group(2))
        data['lat_max'] = parse_latency(m.group(3))

    # Requests/sec
    m = re_req_sec_line.search(text)
    if m:
        data['req_sec'] = float(m.group(1))

    # Transfer/sec
    m = re_transfer_sec_line.search(text)
    if m:
        data['transfer'] = to_bytes(m.group(1), m.group(2))

    # total requests
    m = re_total_reqs_line.search(text)
    if m:
        # strip commas
        data['total_requests'] = int(m.group(1).replace(',', ''))

    # latency distribution: parse lines under 'Latency Distribution' section
    dist = {}
    idx = text.find('Latency Distribution')
    if idx != -1:
        # find the section lines after this
        tail = text[idx:]
        for mm in re_latency_dist.finditer(tail):
            try:
                pct = int(mm.group(1))
                val = parse_latency(mm.group(2))
                dist[pct] = val
            except Exception:
                continue
    for p in (50, 75, 90, 99):
        data[f'lat_p{p}'] = dist.get(p)

    # socket errors
    m = re_socket_errors.search(text)
    if m:
        # parse key values
        parts = [p.strip() for p in m.group(1).split(',')]
        for p in parts:
            if not p: continue
            if ' ' in p:
                k,v = p.split(' ', 1)
                data['socket_' + k] = int(v)
            elif '=' in p:
                k,v = p.split('=',1)
                data['socket_' + k.strip()] = int(v)

    data['raw'] = text
    return data


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage: aggregate_wrk.py <bench_dir>')
        sys.exit(2)

    bench_dir = sys.argv[1]
    if not os.path.isdir(bench_dir):
        print('bench_dir not found:', bench_dir)
        sys.exit(1)

    files = sorted(glob.glob(os.path.join(bench_dir, '*.txt')))
    if not files:
        print('no txt files found in', bench_dir)
        sys.exit(1)

    out_csv = os.path.join(bench_dir, 'report.csv')
    out_json = os.path.join(bench_dir, 'report.json')
    plot_dir = os.path.join(bench_dir, 'plots')
    html_report = os.path.join(bench_dir, 'report.html')
    os.makedirs(plot_dir, exist_ok=True)

    rows = []
    for f in files:
        d = parse_file(f)
        # parse file name for metadata
        base = os.path.basename(f)
        # filename pattern: <endpoint>_t{threads}_c{conns}.txt or __status_t2_c50
        rx = re.match(r"^(.*)_t(\d+)_c(\d+)\.txt$", base)
        if d.get('url'):
            try:
                from urllib.parse import urlparse
                path = urlparse(d.get('url')).path
                d['endpoint'] = path
            except Exception:
                d['endpoint'] = base
        elif rx:
            ep = rx.group(1).replace('_', '/')
            ep = ep.lstrip('/')
            d['endpoint'] = '/' + ep
            d['threads'] = int(rx.group(2)) if not d.get('threads') else d.get('threads')
            d['conns'] = int(rx.group(3)) if not d.get('conns') else d.get('conns')
        else:
            d['endpoint'] = base
        d['file'] = f
        rows.append(d)

    # dump CSV
    # columns to export
    cols = ['file', 'endpoint', 'url', 'threads', 'conns', 'duration', 'total_requests', 'req_sec', 'transfer', 'lat_avg', 'lat_stdev', 'lat_max', 'lat_p50', 'lat_p75', 'lat_p90', 'lat_p99', 'socket_connect', 'socket_read', 'socket_write', 'socket_timeout', 'error']

    with open(out_csv, 'w', newline='') as csvfile:
        w = csv.writer(csvfile)
        w.writerow(cols)
        for r in rows:
            w.writerow([r.get(c) for c in cols])

    # dump JSON
    with open(out_json, 'w') as jf:
        json.dump(rows, jf, indent=2)

    # 生成 per-endpoint 数据（只包含有 req/sec 的行）
    per_ep = defaultdict(list)
    for r in rows:
        ep = r.get('endpoint')
        if r.get('req_sec') is None:
            continue
        per_ep[ep].append(r)

    for ep, ep_rows in per_ep.items():
        # sort by connections
        ep_rows = sorted(ep_rows, key=lambda x: (x.get('threads') or 0, x.get('conns') or 0))
        xs = [ ( (r.get('threads') or 0), (r.get('conns') or 0) ) for r in ep_rows]
        labels = [f"t{t}-c{c}" for t,c in xs]
        reqs = [r.get('req_sec', 0) for r in ep_rows]
        lat50 = [r.get('lat_p50', 0) for r in ep_rows]

        # plot req/sec
        plt.figure(figsize=(8,4))
        plt.plot(labels, reqs, marker='o', linestyle='-')
        plt.title(f'请求速率 (Req/s) - {ep}')
        plt.xlabel('线程-连接')
        plt.ylabel('请求/秒')
        plt.xticks(rotation=45)
        plt.grid(True)
        plt.tight_layout()
        outpng = os.path.join(plot_dir, f'reqsec_{ep.replace("/","_")}.png')
        plt.savefig(outpng)
        plt.close()

        # plot p50 latency (in ms)
        plt.figure(figsize=(8,4))
        lat50_ms = [ (v or 0) * 1000.0 for v in lat50 ]
        plt.plot(labels, lat50_ms, marker='o', linestyle='-', color='orange')
        plt.title(f'P50 延迟 (ms) - {ep}')
        plt.xlabel('线程-连接')
        plt.ylabel('P50 延迟 (ms)')
        plt.xticks(rotation=45)
        plt.grid(True)
        plt.tight_layout()
        outpng2 = os.path.join(plot_dir, f'latp50_{ep.replace("/","_")}.png')
        plt.savefig(outpng2)
        plt.close()

    # Generate HTML report
    html = []
    html.append('<html><head><meta charset="utf-8"><title>wrk 压测报告</title></head><body>')
    html.append('<h1>wrk 压测报告</h1>')
    html.append('<p>生成自: {}</p>'.format(bench_dir))

    # summary per endpoint
    html.append('<h2>端点汇总</h2>')
    html.append('<table border="1" cellspacing="0" cellpadding="3">')
    html.append('<tr><th>端点</th><th>最大请求率(Req/s)</th><th>最佳请求率位置</th><th>最小P50延迟(ms)</th><th>P50最小位置</th></tr>')
    for ep, ep_rows in per_ep.items():
        best_req = max((r.get('req_sec') or 0) for r in ep_rows)
        best_row = max(ep_rows, key=lambda x: (x.get('req_sec') or 0))
        min_p50 = min((r.get('lat_p50') or 9999999) for r in ep_rows)
        min_p50_row = min(ep_rows, key=lambda x: (x.get('lat_p50') or 9999999))
        html.append(f'<tr><td>{ep}</td><td>{best_req:.2f}</td><td>t{best_row.get("threads")}-c{best_row.get("conns")}</td><td>{(min_p50*1000):.2f}</td><td>t{min_p50_row.get("threads")}-c{min_p50_row.get("conns")}</td></tr>')
    html.append('</table>')

    # table
    col_names_zh = {
        'file': '文件',
        'endpoint': '端点',
        'url': 'URL',
        'threads': '线程数',
        'conns': '并发连接数',
        'duration': '持续时间',
        'total_requests': '总请求数',
        'req_sec': '请求/秒',
        'transfer': '吞吐(字节/秒)',
        'lat_avg': '平均延迟(s)',
        'lat_stdev': '延迟标准差(s)',
        'lat_max': '最大延迟(s)',
        'lat_p50': 'P50延迟(s)',
        'lat_p75': 'P75延迟(s)',
        'lat_p90': 'P90延迟(s)',
        'lat_p99': 'P99延迟(s)',
        'socket_connect': 'socket_connect_errors',
        'socket_read': 'socket_read_errors',
        'socket_write': 'socket_write_errors',
        'socket_timeout': 'socket_timeout_errors',
        'error': '错误',
    }
    html.append('<h2>测试结果明细</h2>')
    html.append('<table border="1" cellspacing="0" cellpadding="3">')
    # header row
    html.append('<tr>' + ''.join(f'<th>{col_names_zh.get(c,c)}</th>' for c in cols) + '</tr>')
    for r in rows:
        html.append('<tr>' + ''.join(f'<td>{r.get(c) if r.get(c) is not None else ""}</td>' for c in cols) + '</tr>')
    html.append('</table>')

    # plots
    html.append('<h2>图表</h2>')
    for fn in sorted(glob.glob(os.path.join(plot_dir, '*.png'))):
        html.append(f'<div><h3>{os.path.basename(fn)}</h3><img src="plots/{os.path.basename(fn)}" style="max-width:900px;"/></div>')

    html.append('</body></html>')

    with open(html_report, 'w') as f:
        f.write('\n'.join(html))

    print('报告已生成：')
    print(' - CSV:', out_csv)
    print(' - JSON:', out_json)
    print(' - 图表目录:', plot_dir)
    print(' - HTML 报告:', html_report)
