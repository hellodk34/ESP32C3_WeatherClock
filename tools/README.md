# 字体补字工具使用说明（`gen_font_addchars.py`）

屏幕中文用的是 TFT_eSPI smooth font 字体 `font/msyhl25.h`（约 922 字）。
当屏幕上要显示的字（城市名、天气现象、星期等）**不在字体里**时，该字会显示为空白，
或在切换时残留黑点。本脚本用系统 Noto 字体渲染缺失字，并按「锚定字符」对齐度量后，
合并进现有字体，无需手工改字体二进制。

---

## 一、什么时候需要补字

- 新增了城市（如「北京朝阳」），但「朝」「阳」没在字体里；
- 天气现象出现新的字（如「霾」「雹」等）显示空白。

## 二、环境准备（一次性）

脚本需要 Python 3 + 两个包：`Pillow`、`fontTools`，以及系统 CJK 字体源。

### 2.1 虚拟环境

推荐在项目 `tools/` 下建一个固定的 venv（更持久，不受其它临时目录影响）：

```bash
cd /home/dk/Arduino/projects/ESP32C3_WeatherClock
python3 -m venv tools/.venv
tools/.venv/bin/pip install Pillow fontTools
```

### 2.2 字体源

脚本第 27 行硬编码了字体源（Noto Sans CJK SC，ttc 索引 2）：

```
/usr/share/fonts/opentype/noto/NotoSansCJK-Light.ttc
```

若该路径不存在（换机器/发行版），先安装 Noto CJK（如 `sudo apt install fonts-noto-cjk`），
或改脚本里的 `TTC` / `TTC_INDEX` 指向任意可用的中文 `.ttc/.ttf`。

---

## 三、快速上手（三步）

### 第 1 步：把要补的字加到 `ADD_CHARS`

打开 `tools/gen_font_addchars.py`，找到 `ADD_CHARS` 列表，**追加**你要的字。
每个字用「(字, 锚定字符)」表示：

```python
ADD_CHARS = [
    ("阵", "海"), ("伴", "海"), ("夹", "海"), ("扬", "海"),
    ("飓", "海"), ("热", "海"), ("卷", "海"), ("知", "海"),
    ("-", "0"), ("~", "0"),
    # ↓ 新增示例：北京朝阳的“朝”“阳”
    ("朝", "海"), ("阳", "海"),
]
```

锚定字符规则：

| 要补的字 | 锚定字符 | 说明 |
|---------|---------|------|
| 汉字 | `海` | 继承 `海` 的 advance(25)、按 `海` 的 ink 高度缩放，保证 baseline 对齐 |
| 半角符号/数字 | `0` | 继承数字的 advance(14)，与数字同 baseline |

> **已经补过的字不用删**：脚本会自动跳过「已存在」的字（见下方日志），
> 所以 `ADD_CHARS` 只管往里追加即可，跑多少次都不会重复补。

### 第 2 步：dry-run 预览（不写文件）

```bash
cd /home/dk/Arduino/projects/ESP32C3_WeatherClock
tools/.venv/bin/python tools/gen_font_addchars.py
```

会打印每个字的参数（H/W/adv/dY/dX）。两种结果：

```
[gen] 朝 U+671D (锚定'海'): H=22 W=22 adv=25 dY=20 dX=1 bmp=484B   ← 新补的字
[gen] 阵 U+9635: 已存在，跳过                                     ← 已补过，自动跳过
```

### 第 3 步：确认无误后写入

```bash
tools/.venv/bin/python tools/gen_font_addchars.py --write
```

- 首次写入会自动把原字体备份为 `font/msyhl25.h.bak`（只备份一次，不覆盖旧备份）；
- 写完后重新用 Arduino IDE 编译上传即可。

---

## 四、参数说明

### `ADD_CHARS`：要补的字（含锚定字符）

见「第 1 步」。锚定字符必须是**字体里已存在**的字（`海`、`0` 都在）。

### `OVERRIDE`：手动尺寸覆盖（一般用不到）

连字符、波浪号这类**按 ink 缩放会过细**的符号，需要手动指定尺寸，否则会细到看不见：

```python
OVERRIDE = {
    "-": (9, 3, 10),   # 横杠：9px 长、3px 粗，dY=10（中线对齐数字中线）
    "~": (10, 5, 11),  # 波浪号：10px 长、5px 高，dY=11
}
```

三元组含义为 `(W, H, dY)`：`W`=字宽、`H`=字高、`dY`=字形顶部到 baseline 的距离。
`advance` 仍取锚定字的 advance；`dX` 由 `(adv - W)//2` 自动算出。

> 普通汉字**不需要** OVERRIDE。只有当你 dry-run 后看到某个符号 `H=1`（几乎看不见）时，
> 才给它加一条 OVERRIDE。

---

## 五、验证

写入后，可临时用一段 Python 检查新字是否已在字体里（不装 Pillow 也能跑）：

```bash
python3 - <<'PY'
import re, struct
b = open('font/msyhl25.h','rb').read()
m = re.search(rb'\{([^}]*)\}', b, re.S)
hexes = re.findall(rb'0x([0-9a-fA-F]{2})', m.group(1))
raw = bytes(int(h,16) for h in hexes)
u32 = lambda i: struct.unpack('>I', raw[i:i+4])[0]
gcount = u32(0)
unis = set()
off = 24
for _ in range(gcount):
    unis.add(u32(off)); off += 28
for ch in "朝阳":   # 改成你要查的字
    print(ch, '在字体里' if ord(ch) in unis else '**缺失**')
print('gCount =', gcount)
PY
```

最直接的方式是编译上传后看屏幕：字正常显示即成功。

---

## 六、注意事项

1. **字体格式**：smooth font 头 24 字节（6×uint32 大端：gCount/ver/size/mbox/ascent/descent）
   + 每字形 28 字节描述符 + 紧凑位图（H×W 字节 alpha），字形按 unicode 升序。脚本会自动保持该格式。
2. **ascent=18 的已知缺陷**：部分汉字 `dY` 达 21，顶部会溢出 3px。若补字后出现顶部残留黑点，
   那是布局清屏范围的问题（不是补字脚本的问题），需在对应 `fillRect` 里向上扩 3px 覆盖。
3. **备份**：原字体备份在 `font/msyhl25.h.bak`（`--write` 首次运行时生成），如需回滚把它复制回 `msyhl25.h` 即可。
4. **锚定字符一致性**：同批补字尽量用同一锚定字符，保证新字之间风格、基线一致。
