#!/usr/bin/env python3
"""
verify_screenshots.py — 验证 HeadlessPluginHost 的 UI 截图功能是否已修复。

测试流程:
  1. 运行 host 执行截图命令 (effect + synth 两个插件)
  2. 读取生成的 PNG 图片
  3. 统计像素分布，判断是否为纯色图片
  4. 输出详细诊断信息

判定标准:
  - 唯一颜色数 > 10        → 内容丰富 (PASS)
  - 非黑像素比例 > 5%      → 非纯黑   (PASS)
  - 标准差 (亮度) > 3.0    → 有对比度 (PASS)
  三项全部通过才算修复成功。
"""

import json
import os
import platform
import struct
import subprocess
import sys
import tempfile
import zlib

# ============================================================================
# 纯 Python PNG 解码器 (不依赖 Pillow / numpy)
# ============================================================================

def _read_png(path: str):
    """最小化 PNG 解码——返回 (width, height, pixels)。
    pixels 是 [(r,g,b,a), ...] 列表，长度 = width * height。
    仅支持 RGBA / RGB, 8-bit, non-interlaced。"""
    with open(path, "rb") as f:
        sig = f.read(8)
        if sig != b'\x89PNG\r\n\x1a\n':
            raise ValueError("Not a PNG file")

        chunks = {}
        idat_data = b""
        width = height = 0
        color_type = bit_depth = 0

        while True:
            raw = f.read(8)
            if len(raw) < 8:
                break
            length, ctype = struct.unpack(">I4s", raw)
            data = f.read(length)
            _crc = f.read(4)

            ctype_str = ctype.decode("ascii", errors="replace")
            if ctype_str == "IHDR":
                width, height, bit_depth, color_type = struct.unpack(">IIBB", data[:10])
            elif ctype_str == "IDAT":
                idat_data += data
            elif ctype_str == "IEND":
                break

        raw_data = zlib.decompress(idat_data)

        # bytes per pixel
        if color_type == 6:    # RGBA
            bpp = 4
        elif color_type == 2:  # RGB
            bpp = 3
        else:
            raise ValueError(f"Unsupported PNG color_type={color_type}")

        stride = 1 + width * bpp  # 1 byte filter per row
        pixels = []
        prev_row = bytes(width * bpp)

        for y in range(height):
            row_start = y * stride
            filt = raw_data[row_start]
            row = bytearray(raw_data[row_start + 1 : row_start + stride])

            # Reconstruct (filter types 0-4)
            for i in range(len(row)):
                a = row[i - bpp] if i >= bpp else 0
                b = prev_row[i]
                c = prev_row[i - bpp] if i >= bpp else 0
                if filt == 0:
                    pass
                elif filt == 1:
                    row[i] = (row[i] + a) & 0xFF
                elif filt == 2:
                    row[i] = (row[i] + b) & 0xFF
                elif filt == 3:
                    row[i] = (row[i] + (a + b) // 2) & 0xFF
                elif filt == 4:
                    # Paeth
                    p = a + b - c
                    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                    if pa <= pb and pa <= pc:
                        pr = a
                    elif pb <= pc:
                        pr = b
                    else:
                        pr = c
                    row[i] = (row[i] + pr) & 0xFF

            prev_row = bytes(row)

            for x in range(width):
                off = x * bpp
                r, g, b_ = row[off], row[off + 1], row[off + 2]
                a_val = row[off + 3] if bpp == 4 else 255
                pixels.append((r, g, b_, a_val))

        return width, height, pixels


def analyze_image(path: str) -> dict:
    """Analyze a PNG and return diagnostic metrics."""
    w, h, pixels = _read_png(path)
    total = len(pixels)

    # Unique colors (sample at most 200k pixels for speed)
    step = max(1, total // 200000)
    sampled = pixels[::step]
    unique_colors = len(set(sampled))

    # Count non-black pixels (r+g+b > 10)
    non_black = sum(1 for (r, g, b, a) in pixels if r + g + b > 10)
    non_black_pct = non_black / total * 100

    # Luminance stats
    lums = [(0.299 * r + 0.587 * g + 0.114 * b) for (r, g, b, a) in pixels]
    mean_lum = sum(lums) / total
    var = sum((l - mean_lum) ** 2 for l in lums) / total
    std_lum = var ** 0.5

    # Top 5 most common colors (sampled)
    from collections import Counter
    counter = Counter(sampled)
    top5 = counter.most_common(5)

    return {
        "path": path,
        "width": w,
        "height": h,
        "total_pixels": total,
        "unique_colors_sampled": unique_colors,
        "non_black_pixels": non_black,
        "non_black_pct": round(non_black_pct, 2),
        "mean_luminance": round(mean_lum, 2),
        "std_luminance": round(std_lum, 2),
        "top5_colors": [{"rgba": c, "count": n} for c, n in top5],
    }


def judge(metrics: dict) -> tuple:
    """Return (pass: bool, reasons: list[str])."""
    reasons = []
    ok = True

    if metrics["unique_colors_sampled"] <= 10:
        reasons.append(f"唯一颜色数 {metrics['unique_colors_sampled']} ≤ 10 → 几乎纯色")
        ok = False
    else:
        reasons.append(f"唯一颜色数 {metrics['unique_colors_sampled']} > 10 ✓")

    if metrics["non_black_pct"] <= 5.0:
        reasons.append(f"非黑像素 {metrics['non_black_pct']}% ≤ 5% → 几乎纯黑")
        ok = False
    else:
        reasons.append(f"非黑像素 {metrics['non_black_pct']}% > 5% ✓")

    if metrics["std_luminance"] <= 3.0:
        reasons.append(f"亮度标准差 {metrics['std_luminance']} ≤ 3.0 → 无对比度")
        ok = False
    else:
        reasons.append(f"亮度标准差 {metrics['std_luminance']} > 3.0 ✓")

    return ok, reasons


# ============================================================================
# Main
# ============================================================================
def main():
    project_dir = os.path.dirname(os.path.abspath(__file__))
    is_windows = platform.system() == "Windows"

    host_name = "HeadlessPluginHost.exe" if is_windows else "HeadlessPluginHost"
    host_bin = os.path.join(
        project_dir, "build", "HeadlessPluginHost_artefacts", "Release", host_name
    )

    effect_vst = os.path.join(
        project_dir, "build", "TestGainPlugin_artefacts", "Release", "VST3", "Test Gain Plugin.vst3"
    )
    synth_vst = os.path.join(
        project_dir, "build", "TestSynthPlugin_artefacts", "Release", "VST3", "Test Synth Plugin.vst3"
    )

    with tempfile.TemporaryDirectory() as tmpdir:
        effect_png = os.path.join(tmpdir, "effect.png")
        synth_png  = os.path.join(tmpdir, "synth.png")

        commands = [
            {"action": "load_plugin", "plugin_path": effect_vst},
            {"action": "open_editor"},
            {"action": "capture_ui", "output_path": effect_png, "width": 500, "height": 350},
            {"action": "close_editor"},
            {"action": "load_plugin", "plugin_path": synth_vst},
            {"action": "open_editor"},
            {"action": "capture_ui", "output_path": synth_png, "width": 600, "height": 400},
            {"action": "close_editor"},
        ]

        cmd_file = os.path.join(tmpdir, "cmds.json")
        with open(cmd_file, "w") as f:
            json.dump(commands, f)

        # Run the host (use xvfb-run on Linux, direct execution on Windows/macOS)
        print("=" * 60)
        print("  HeadlessPluginHost 截图验证脚本")
        print("=" * 60)
        print(f"\n[1/3] 运行 host 生成截图 ...")

        if is_windows or platform.system() == "Darwin":
            run_cmd = [host_bin, "--file", cmd_file]
        else:
            run_cmd = ["xvfb-run", "-a", host_bin, "--file", cmd_file]

        result = subprocess.run(
            run_cmd,
            capture_output=True, text=True, cwd=project_dir, timeout=60
        )

        # Parse JSON output
        stdout = result.stdout
        start = stdout.find("{")
        end = stdout.rfind("}") + 1
        if start < 0 or end <= start:
            print("  ❌ Host 输出无法解析为 JSON")
            print(stdout[:500])
            sys.exit(1)

        host_output = json.loads(stdout[start:end])

        # Check for command failures
        for r in host_output.get("results", []):
            act = r.get("action", "")
            if act in ("capture_ui", "open_editor", "close_editor", "load_plugin"):
                if not r.get("success", False):
                    print(f"  ❌ {act} 失败: {r.get('error', '?')}")
                    sys.exit(1)

        print("  ✓ Host 命令全部成功\n")

        # Analyze each screenshot
        print("[2/3] 分析截图像素 ...\n")
        all_pass = True
        for label, png_path in [("Effect Plugin", effect_png), ("Synth Plugin", synth_png)]:
            if not os.path.isfile(png_path):
                print(f"  ❌ {label}: 文件不存在 ({png_path})")
                all_pass = False
                continue

            fsize = os.path.getsize(png_path)
            print(f"  --- {label} ({fsize} bytes) ---")
            metrics = analyze_image(png_path)
            passed, reasons = judge(metrics)

            print(f"    分辨率:       {metrics['width']}x{metrics['height']}")
            print(f"    唯一颜色(采样): {metrics['unique_colors_sampled']}")
            print(f"    非黑像素:     {metrics['non_black_pct']}%  ({metrics['non_black_pixels']}/{metrics['total_pixels']})")
            print(f"    平均亮度:     {metrics['mean_luminance']}")
            print(f"    亮度标准差:   {metrics['std_luminance']}")
            print(f"    Top-5 颜色:   {[c['rgba'][:3] for c in metrics['top5_colors']]}")
            print()
            for r in reasons:
                print(f"    {r}")
            verdict = "✅ PASS" if passed else "❌ FAIL"
            print(f"    结果: {verdict}")
            print()
            if not passed:
                all_pass = False

        # Final verdict
        print("=" * 60)
        if all_pass:
            print("  🎉 截图功能验证通过 — 图像包含有效 UI 内容！")
            print("=" * 60)
            sys.exit(0)
        else:
            print("  💀 截图功能仍有问题 — 图像可能是纯色/纯黑。")
            print("=" * 60)
            sys.exit(1)


if __name__ == "__main__":
    main()
