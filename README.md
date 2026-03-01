# HeadlessPluginHost — AI 驱动的无头 VST3 插件调试宿主

> 一个专门为 AI Agent 自动化调试设计的命令行 VST3 宿主工具。  
> 通过 JSON 命令协议与 AI 交互：加载插件 → 注入信号 → 分析输出 → 截取 UI → 压力测试生命周期。

---

## 目录

- [快速开始](#快速开始)
- [构建](#构建)
- [运行方式](#运行方式)
- [命令参考（完整）](#命令参考完整)
  - [插件管理](#1-插件管理)
  - [参数控制](#2-参数控制)
  - [音频处理（效果器）](#3-音频处理效果器)
  - [MIDI / 合成器](#4-midi--合成器)
  - [UI 截图与布局](#5-ui-截图与布局)
  - [生命周期压力测试](#6-生命周期压力测试)
- [输出格式](#输出格式)
- [信号类型一览](#信号类型一览)
- [典型 AI 工作流示例](#典型-ai-工作流示例)
- [项目结构](#项目结构)
- [平台说明](#平台说明)

---

## 快速开始

```bash
# 1. 构建
cd Plugin_host
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 2. 快速测试（一行命令）
xvfb-run -a ./build/HeadlessPluginHost_artefacts/Release/HeadlessPluginHost \
  --plugin path/to/plugin.vst3 --signal sine --frequency 440 --duration 1000

# 3. 批处理模式（推荐 AI 使用）
xvfb-run -a ./build/HeadlessPluginHost_artefacts/Release/HeadlessPluginHost \
  --file commands.json
```

---

## 构建

### 系统要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Linux (Ubuntu 20.04+)、macOS、Windows |
| 编译器 | GCC 10+ / Clang 12+ / MSVC 2019+ |
| CMake | 3.22+ |
| C++ 标准 | C++17 |

### Linux 依赖安装

```bash
sudo apt-get install -y \
  build-essential cmake \
  libasound2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev \
  libwebkit2gtk-4.1-dev libcurl4-openssl-dev \
  xvfb
```

### 编译步骤

```bash
cd Plugin_host
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

构建产物：

| 文件 | 说明 |
|------|------|
| `build/HeadlessPluginHost_artefacts/Release/HeadlessPluginHost` | 宿主可执行文件 |
| `build/TestGainPlugin_artefacts/Release/VST3/Test Gain Plugin.vst3` | 测试效果器插件 |
| `build/TestSynthPlugin_artefacts/Release/VST3/Test Synth Plugin.vst3` | 测试合成器插件 |

---

## 运行方式

> **重要**：在 Linux 无桌面环境下，必须用 `xvfb-run -a` 包裹执行命令，为 VST3 插件提供虚拟 X 服务器。

### 模式 1：批处理文件（推荐 AI 使用）

```bash
xvfb-run -a ./HeadlessPluginHost --file commands.json
```

`commands.json` 是一个 JSON 数组，每个元素是一条命令对象：

```json
[
  {"action": "load_plugin", "plugin_path": "path/to/plugin.vst3"},
  {"action": "get_plugin_info"},
  {"action": "generate_and_process", "signal_type": "sine", "frequency": 440, "duration_ms": 500}
]
```

### 模式 2：标准输入

```bash
echo '[{"action":"load_plugin","plugin_path":"plugin.vst3"},{"action":"get_plugin_info"}]' | \
  xvfb-run -a ./HeadlessPluginHost --stdin
```

### 模式 3：快速单插件测试

```bash
xvfb-run -a ./HeadlessPluginHost --plugin plugin.vst3 \
  --signal sine --frequency 1000 --duration 500 \
  --sample-rate 48000 --block-size 256 \
  --screenshot /tmp/ui.png
```

快速模式参数：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--signal <type>` | impulse | 信号类型：impulse / step / sine / sweep / noise |
| `--frequency <Hz>` | 440 | 正弦波频率 |
| `--duration <ms>` | 1000 | 信号时长 |
| `--sample-rate <Hz>` | 44100 | 采样率 |
| `--block-size <n>` | 512 | 处理块大小 |
| `--screenshot <path>` | (无) | UI 截图输出路径 |

### 输出约定

- **stdout** → 纯 JSON 结果（用于 AI 解析）
- **stderr** → 诊断日志（`[HeadlessHost] ...`）

AI Agent 应只解析 stdout 的 JSON，忽略 stderr。

---

## 命令参考（完整）

所有命令通过 `"action"` 字段指定动作名称。命令名**不区分大小写**。

---

### 1. 插件管理

#### `load_plugin` — 加载 VST3 插件

```json
{
  "action": "load_plugin",
  "plugin_path": "path/to/plugin.vst3",
  "sample_rate": 44100,
  "block_size": 512
}
```

| 字段 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `plugin_path` | string | ✅ | — | VST3 文件路径（绝对或相对于工作目录） |
| `sample_rate` | number | — | 44100 | 采样率 (Hz) |
| `block_size` | number | — | 512 | 处理块大小 (samples) |

**返回数据**：

```json
{
  "plugin_name": "My Plugin",
  "plugin_format": "VST3",
  "manufacturer": "Company",
  "version": "1.0.0",
  "category": "Fx",
  "num_input_channels": 2,
  "num_output_channels": 2,
  "latency_samples": 0,
  "accepts_midi": false,
  "produces_midi": false,
  "is_synth": false,
  "has_editor": true,
  "sample_rate": 44100.0,
  "block_size": 512,
  "num_parameters": 5,
  "num_programs": 0,
  "current_program": 0
}
```

#### `unload_plugin` — 卸载当前插件

```json
{"action": "unload_plugin"}
```

无额外参数。会自动关闭编辑器并释放所有资源。

#### `get_plugin_info` — 获取插件信息

```json
{"action": "get_plugin_info"}
```

返回与 `load_plugin` 相同的插件描述数据。

---

### 2. 参数控制

#### `list_parameters` — 列出所有参数

```json
{"action": "list_parameters"}
```

**返回数据**：

```json
{
  "num_parameters": 4,
  "parameters": [
    {
      "index": 0,
      "parameter_id": "gain",
      "name": "Gain",
      "value_normalized": 0.5,
      "value_text": "0.0 dB",
      "default_value": 0.5,
      "num_steps": 0,
      "is_automatable": true,
      "is_discrete": false,
      "is_boolean": false,
      "label": "dB"
    }
  ]
}
```

#### `set_parameter` — 设置参数值

```json
{
  "action": "set_parameter",
  "parameter_id": "gain",
  "value": 0.75
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `parameter_id` | string | ✅ (二选一) | 参数 ID 字符串 |
| `index` | number | ✅ (二选一) | 参数索引（从 0 开始） |
| `value` | number | ✅ | 归一化值 (0.0 ~ 1.0) |

#### `get_parameter` — 读取参数值

```json
{
  "action": "get_parameter",
  "parameter_id": "gain"
}
```

---

### 3. 音频处理（效果器）

#### `generate_and_process` — 生成信号并通过插件处理

```json
{
  "action": "generate_and_process",
  "signal_type": "sine",
  "frequency": 440.0,
  "duration_ms": 500,
  "amplitude": 0.8,
  "sample_rate": 44100
}
```

| 字段 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `signal_type` | string | ✅ | — | 信号类型（见[信号类型一览](#信号类型一览)） |
| `duration_ms` | number | — | 1000 | 信号时长 (ms) |
| `frequency` | number | — | 440 | 频率 (Hz)，用于 sine / sweep |
| `end_frequency` | number | — | 20000 | 扫频终止频率 (Hz)，用于 sweep |
| `amplitude` | number | — | 1.0 | 振幅 (0.0 ~ 1.0) |
| `file_path` | string | — | — | 音频文件路径，用于 file 类型 |

**返回数据**包含自动 FFT 分析：

```json
{
  "input_samples": 22050,
  "output_samples": 22050,
  "output_channels": 2,
  "signal_type": "sine",
  "analysis": {
    "channels": [
      {
        "channel": 0,
        "rms_db": -6.02,
        "peak_db": -0.01,
        "has_nan": false,
        "has_inf": false,
        "dc_offset": 0.0001
      }
    ]
  }
}
```

#### `analyze_output` — 对上次处理结果做详细 FFT 分析

```json
{
  "action": "analyze_output",
  "include_spectrum": true,
  "fft_order": 12,
  "max_peaks": 5
}
```

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `include_spectrum` | bool | false | 是否包含完整频谱数据 |
| `fft_order` | number | 12 | FFT 阶数 (8~16, 即 256~65536 点) |
| `max_peaks` | number | 10 | 报告的频谱峰值最大数量 |

**返回数据**（当 `include_spectrum=true`）：

```json
{
  "analysis": {
    "channels": [
      {
        "channel": 0,
        "rms_db": -6.02,
        "peak_db": 0.0,
        "has_nan": false,
        "has_inf": false,
        "spectral_peaks": [
          {"frequency_hz": 440.0, "magnitude_db": -3.1, "bin_index": 41}
        ],
        "spectrum": [0.001, 0.002, ...]
      }
    ]
  }
}
```

---

### 4. MIDI / 合成器

#### `render_synth_note` — 便捷：渲染单个合成器音符

```json
{
  "action": "render_synth_note",
  "note": 69,
  "velocity": 0.8,
  "duration_ms": 500,
  "release_ms": 100
}
```

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `note` | number | 60 | MIDI 音符号 (0~127, 60=C4, 69=A4) |
| `velocity` | number | 0.8 | 力度 (0.0~1.0) |
| `duration_ms` | number | 1000 | 总渲染时长 (ms) |
| `release_ms` | number | 100 | note-off 距结尾的提前量 (ms) |

**返回数据**：

```json
{
  "note_number": 69,
  "velocity": 0.8,
  "duration_ms": 500.0,
  "release_ms": 100.0,
  "expected_freq_hz": 440.0,
  "output_samples": 22050,
  "output_channels": 2,
  "analysis": { "channels": [...] }
}
```

#### `send_midi_and_process` — 底层：发送自定义 MIDI 序列

```json
{
  "action": "send_midi_and_process",
  "duration_ms": 600,
  "sample_rate": 44100,
  "midi_events": [
    {"type": "note_on",  "note": 60, "velocity": 0.8, "time_ms": 0},
    {"type": "note_on",  "note": 64, "velocity": 0.7, "time_ms": 0},
    {"type": "cc",       "cc_number": 1, "cc_value": 64, "time_ms": 100},
    {"type": "note_off", "note": 60, "velocity": 0,   "time_ms": 400},
    {"type": "note_off", "note": 64, "velocity": 0,   "time_ms": 400}
  ]
}
```

**MIDI 事件类型**：

| type | 必填字段 | 说明 |
|------|----------|------|
| `note_on` | `note`, `velocity` | MIDI Note-On |
| `note_off` | `note` | MIDI Note-Off |
| `cc` / `control_change` | `cc_number`, `cc_value` | CC 控制器 |
| `pitch_bend` | `bend_value` (0~16383, 8192=中) | 弯音轮 |

**所有事件通用字段**：

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `channel` | number | 1 | MIDI 通道 (1~16) |
| `time_ms` | number | 0 | 事件时间点 (ms) |
| `note` | number | 60 | MIDI 音符号 |
| `velocity` | number | 0.8 | 力度 (0.0~1.0, 内部转换为 0~127) |

---

### 5. UI 截图与布局

#### `open_editor` — 打开插件编辑器窗口

```json
{"action": "open_editor"}
```

**返回数据**：

```json
{
  "editor_width": 500,
  "editor_height": 350,
  "open_time_ms": 108.5
}
```

#### `close_editor` — 关闭编辑器窗口

```json
{"action": "close_editor"}
```

幂等操作——如果编辑器未打开也不会报错。

#### `capture_ui` — 截取插件 UI 为 PNG 图片

```json
{
  "action": "capture_ui",
  "output_path": "/tmp/screenshot.png",
  "width": 800,
  "height": 600
}
```

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `output_path` | string | screenshot.png | PNG 输出路径 |
| `width` | number | 800 | 截图宽度 (px) |
| `height` | number | 600 | 截图高度 (px) |

> **提示**：如果之前已调用 `open_editor`，截图会复用已打开的编辑器实例。
> 也可以在不先调用 `open_editor` 的情况下直接使用（会自动创建临时编辑器）。

**返回数据**：

```json
{
  "output_path": "/tmp/screenshot.png",
  "width": 800,
  "height": 600,
  "file_size_bytes": 20144
}
```

#### `get_parameter_layout` — 导出组件树与参数映射

```json
{
  "action": "get_parameter_layout",
  "width": 800,
  "height": 600
}
```

**返回数据**包含完整的 UI 组件树：

```json
{
  "editor_width": 800,
  "editor_height": 600,
  "component_tree": {
    "name": "unnamed",
    "type": "VST3PluginWindow",
    "visible": true,
    "bounds": {"x": 0, "y": 0, "width": 800, "height": 600},
    "children": [
      {
        "name": "GainSlider",
        "widget_type": "slider",
        "visible": true,
        "bounds": {"x": 15, "y": 70, "width": 150, "height": 230}
      },
      {
        "name": "GainLabel",
        "widget_type": "label",
        "text": "Gain (dB)",
        "bounds": {"x": 10, "y": 50, "width": 160, "height": 20}
      }
    ]
  },
  "parameters": [
    {"index": 0, "parameter_id": "gain", "name": "Gain", "value_normalized": 0.5, "value_text": "0.0 dB"}
  ]
}
```

---

### 6. 生命周期压力测试

#### `test_plugin_lifecycle` — 插件加载/卸载循环测试

```json
{
  "action": "test_plugin_lifecycle",
  "plugin_path": "path/to/plugin.vst3",
  "iterations": 10,
  "sample_rate": 44100,
  "block_size": 512
}
```

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `plugin_path` | string | ✅ | VST3 路径 |
| `iterations` | number | 5 | 循环次数 (1~100) |
| `sample_rate` | number | 44100 | 采样率 |
| `block_size` | number | 512 | 块大小 |

每次迭代执行：`loadPlugin → processBlock(silence) → unloadPlugin`。

**返回数据**：

```json
{
  "iterations": 10,
  "success_count": 10,
  "fail_count": 0,
  "avg_load_ms": 14.6,
  "avg_unload_ms": 5.0,
  "all_passed": true,
  "details": [
    {
      "iteration": 0,
      "load_success": true,
      "load_time_ms": 18.7,
      "process_ok": true,
      "unload_time_ms": 4.8,
      "clean_unload": true
    }
  ]
}
```

#### `test_editor_lifecycle` — 编辑器开关循环测试

```json
{
  "action": "test_editor_lifecycle",
  "iterations": 10,
  "delay_between_ms": 50
}
```

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `iterations` | number | 5 | 循环次数 (1~100) |
| `delay_between_ms` | number | 50 | 每次 open/close 之间的延迟 (ms) |

> **前置条件**：必须先 `load_plugin`。

每次迭代执行：`openEditor → (delay) → closeEditor`。

**返回数据**：

```json
{
  "iterations": 10,
  "success_count": 10,
  "fail_count": 0,
  "avg_open_ms": 101.0,
  "avg_close_ms": 100.2,
  "all_passed": true,
  "details": [
    {
      "iteration": 0,
      "open_success": true,
      "open_time_ms": 100.7,
      "close_success": true,
      "close_time_ms": 100.2
    }
  ]
}
```

---

## 输出格式

### 批处理输出结构

```json
{
  "version": "1.0",
  "total_commands": 5,
  "results": [
    {
      "action": "load_plugin",
      "success": true,
      "data": { ... },
      "command_index": 0
    },
    {
      "action": "set_parameter",
      "success": false,
      "error": "Parameter not found: foobar",
      "command_index": 1
    }
  ]
}
```

### 每条结果的通用字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `action` | string | 执行的命令名 |
| `success` | bool | 是否成功 |
| `data` | object | 成功时的返回数据 |
| `error` | string | 失败时的错误信息 |
| `command_index` | number | 命令在批次中的索引（从 0 开始） |

### 崩溃输出

如果插件导致宿主崩溃（SIGSEGV/SIGABRT/SIGFPE/SIGBUS），会输出：

```json
{
  "version": "1.0",
  "fatal_error": true,
  "signal": 11,
  "signal_name": "SIGSEGV (Segmentation fault)",
  "message": "Plugin caused a fatal crash"
}
```

---

## 信号类型一览

在 `generate_and_process` 命令的 `signal_type` 字段中使用：

| signal_type | 说明 | 关键参数 |
|-------------|------|----------|
| `impulse` | 冲激信号（Dirac delta） | `amplitude` |
| `step` | 阶跃信号 | `amplitude` |
| `sine` | 正弦波 | `frequency`, `amplitude` |
| `sweep` | 线性扫频 | `frequency`(起始), `end_frequency`(终止), `amplitude` |
| `noise` | 白噪声 | `amplitude` |
| `file` | 从文件加载 | `file_path` |

---

## 典型 AI 工作流示例

### 示例 1：验证增益插件是否正确衰减信号

```json
[
  {"action": "load_plugin", "plugin_path": "MyGain.vst3"},
  {"action": "set_parameter", "parameter_id": "gain", "value": 0.0},
  {"action": "generate_and_process", "signal_type": "sine", "frequency": 1000, "duration_ms": 200},
  {"action": "analyze_output", "include_spectrum": false}
]
```

**AI 应检查**：`analysis.channels[0].rms_db` 应 ≈ -inf（增益归零 → 静音）。

### 示例 2：检测合成器是否能发出正确频率的声音

```json
[
  {"action": "load_plugin", "plugin_path": "MySynth.vst3"},
  {"action": "render_synth_note", "note": 69, "velocity": 0.9, "duration_ms": 500},
  {"action": "analyze_output", "include_spectrum": true, "max_peaks": 3}
]
```

**AI 应检查**：`spectral_peaks[0].frequency_hz` 应 ≈ 440 Hz（A4 音符）。

### 示例 3：检测编辑器内存泄漏

```json
[
  {"action": "load_plugin", "plugin_path": "MyPlugin.vst3"},
  {"action": "test_editor_lifecycle", "iterations": 50, "delay_between_ms": 10}
]
```

**AI 应检查**：`all_passed == true`，且 `avg_open_ms` 在各迭代间不持续增长。

### 示例 4：UI 审计工作流

```json
[
  {"action": "load_plugin", "plugin_path": "MyPlugin.vst3"},
  {"action": "open_editor"},
  {"action": "capture_ui", "output_path": "/tmp/ui_default.png", "width": 800, "height": 600},
  {"action": "set_parameter", "parameter_id": "gain", "value": 1.0},
  {"action": "capture_ui", "output_path": "/tmp/ui_max_gain.png"},
  {"action": "get_parameter_layout"},
  {"action": "close_editor"}
]
```

**AI 应检查**：比较两张截图差异，验证 UI 响应参数变化；检查组件树中是否有预期的控件。

### 示例 5：MIDI 和弦与 CC 自动化

```json
[
  {"action": "load_plugin", "plugin_path": "MySynth.vst3"},
  {"action": "send_midi_and_process", "duration_ms": 1000, "midi_events": [
    {"type": "note_on",  "note": 60, "velocity": 0.8, "time_ms": 0},
    {"type": "note_on",  "note": 64, "velocity": 0.7, "time_ms": 0},
    {"type": "note_on",  "note": 67, "velocity": 0.6, "time_ms": 0},
    {"type": "cc",       "cc_number": 7, "cc_value": 127, "time_ms": 0},
    {"type": "cc",       "cc_number": 7, "cc_value": 0,   "time_ms": 500},
    {"type": "note_off", "note": 60, "time_ms": 800},
    {"type": "note_off", "note": 64, "time_ms": 800},
    {"type": "note_off", "note": 67, "time_ms": 800}
  ]},
  {"action": "analyze_output", "include_spectrum": true}
]
```

**AI 应检查**：频谱中应有 C4(261Hz)、E4(330Hz)、G4(392Hz) 三个峰；CC7 淡出应导致后半段 RMS 降低。

---

## 项目结构

```
Plugin_host/
├── CMakeLists.txt              # 顶层构建配置
├── README.md                   # 本文档
├── test_commands_v2.json       # 完整测试命令集
├── verify_screenshots.py       # 截图验证脚本
│
├── src/                        # 宿主源码
│   ├── Main.cpp                # 入口：CLI 解析、崩溃处理、X11 初始化
│   ├── PluginHost.h/cpp        # 核心：插件加载/卸载/处理/MIDI/生命周期
│   ├── BatchProcessor.h/cpp    # JSON 命令分发器
│   ├── SignalGenerator.h/cpp   # 多源测试信号生成
│   ├── AudioAnalyzer.h/cpp     # FFT + RMS/Peak 分析
│   ├── OffscreenRenderer.h/cpp # UI 截图 (X11 XGetImage) + 组件树
│   └── JsonHelper.h            # JSON 序列化工具
│
├── test_plugin/                # 测试效果器 (Gain + Tone + Mix)
│   ├── PluginProcessor.h/cpp
│   └── PluginEditor.h/cpp
│
├── test_synth/                 # 测试合成器 (8-voice poly, ADSR)
│   ├── SynthProcessor.h/cpp
│   └── SynthEditor.h/cpp
│
└── JUCE/                       # JUCE 8.x 框架
```

---

## 平台说明

### Linux（主要开发平台）

- 必须使用 `xvfb-run -a` 运行（即使不需要 UI 截图，VST3 插件也可能需要 X11 display）
- UI 截图通过 X11 `XGetImage` 从虚拟 X 服务器抓取合成像素，**可以正确捕获 XEmbed 子窗口内容**
- 已内置 X11 Atom 预注册和静默错误处理器，避免无窗口管理器时的 BadAtom 崩溃

### macOS / Windows

- 源码跨平台兼容，无需修改
- UI 截图在有窗口服务器的平台上会回退到 JUCE 的 `createComponentSnapshot()`
- 不需要 xvfb

### 已知限制

- 同一时刻只能加载一个插件实例
- UI 截图在 Linux 上需要约 400ms 延迟等待 X 服务器渲染
- 仅支持 VST3 格式（不支持 AU / LV2 / VST2）

---

## 命令速查表

| 命令 | 用途 | 需要插件 | 需要编辑器 |
|------|------|---------|-----------|
| `load_plugin` | 加载 VST3 | — | — |
| `unload_plugin` | 卸载插件 | — | — |
| `get_plugin_info` | 获取插件信息 | ✅ | — |
| `list_parameters` | 列出参数 | ✅ | — |
| `set_parameter` | 设置参数 | ✅ | — |
| `get_parameter` | 读取参数 | ✅ | — |
| `generate_and_process` | 信号处理 | ✅ | — |
| `analyze_output` | FFT 分析 | ✅ (先 process) | — |
| `open_editor` | 打开编辑器 | ✅ | — |
| `close_editor` | 关闭编辑器 | — | — |
| `capture_ui` | UI 截图 | ✅ | 可选 |
| `get_parameter_layout` | 组件树导出 | ✅ | 可选 |
| `render_synth_note` | 渲染音符 | ✅ | — |
| `send_midi_and_process` | MIDI 序列处理 | ✅ | — |
| `test_plugin_lifecycle` | 加载/卸载压测 | — | — |
| `test_editor_lifecycle` | 编辑器开关压测 | ✅ | — |
