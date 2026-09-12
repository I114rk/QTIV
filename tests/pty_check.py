#!/usr/bin/env python3
"""Тест консольного вывода (qtiv -c) через pty-эмуляцию терминала.

Запускает qtiv внутри псевдотерминала, отвечает на escape-запросы
(kitty graphics / DA1) и проверяет, какой режим вывода был выбран:
  - терминал без sixel, отвечающий DA1 уровня 64, не должен получить sixel
    (регрессия: парсер DA1 съедал первый символ параметров);
  - внутри tmux (env TMUX) kitty/sixel не работают — должны быть полублоки;
  - kitty-передача должна содержать валидный PNG.

Запуск: python3 tests/pty_check.py [путь-к-qtiv]
"""
import base64
import fcntl
import os
import pty
import re
import select
import struct
import subprocess
import sys
import termios
import time

BIN = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else "./build/qtiv"
IMG = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   "..", "examples", "pattern.png"))


def run(env_extra, args, responses=None, timeout=5.0):
    """responses: {pattern: reply} — отправить reply в pty, когда qtiv выдал pattern."""
    responses = dict(responses or {})
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 120, 720, 1280))
    env = dict(os.environ)
    env.update(env_extra)
    proc = subprocess.Popen([BIN] + args, stdin=slave, stdout=slave, stderr=slave,
                            env=env, close_fds=True)
    os.close(slave)

    out = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        r, _, _ = select.select([master], [], [], 0.05)
        if r:
            try:
                chunk = os.read(master, 65536)
            except OSError:
                break
            if not chunk:
                break
            out += chunk
            for pat, reply in responses.items():
                if reply is not None and pat in out:
                    os.write(master, reply)
                    responses[pat] = None  # отвечаем один раз
        elif proc.poll() is not None:
            try:
                while True:
                    chunk = os.read(master, 65536)
                    if not chunk:
                        break
                    out += chunk
            except OSError:
                pass
            break
    os.close(master)
    proc.wait()
    return proc.returncode, out


def describe(out):
    if b"\x1b_Gf=100" in out or re.search(rb"\x1b_Gf=24;", out):
        return "KITTY-IMAGE"
    if b"\x1bPq" in out:
        return "SIXEL"
    if "▀".encode() in out:
        return "HALFBLOCKS"
    if b"\x1b[38" in out or b"\x1b[48" in out:
        return "COLORED-ASCII"
    return "PLAIN-ASCII"


def kitty_payload(out):
    """PNG из kitty-последовательности (одно- или многочанковой) или None.

    Первый чанк имеет параметры с f=100; чанки-продолжения — только m=0/1.
    Запрос определения возможностей (a=q, payload AAAA) не матчится.
    """
    firsts = re.findall(rb"\x1b_Gf=100[^;]*;([A-Za-z0-9+/=]*)\x1b\\", out)
    conts = re.findall(rb"\x1b_Gm=[01];([A-Za-z0-9+/=]*)\x1b\\", out)
    if not firsts:
        return None
    try:
        return base64.b64decode(b"".join(firsts) + b"".join(conts))
    except Exception:
        return None


def validate_sixel(out):
    """Структурная проверка sixel-потока: возвращает (ok, описание)."""
    m = re.search(rb"\x1bPq(.*?)\x1b\\", out, re.S)
    if not m:
        return False, "sixel DCS not found"
    body = m.group(1)

    # Растер: "pan;pad;ph;pv — размеры должны быть заданы и ненулевые.
    rm = re.match(rb'"(\d+);(\d+);(\d+);(\d+)', body)
    if not rm:
        # атрибут может идти после цветовых определений — ищем где угодно
        rm = re.search(rb'"(\d+);(\d+);(\d+);(\d+)', body)
    if not rm:
        return False, "raster attributes missing"
    width, height = int(rm.group(3)), int(rm.group(4))
    if width <= 0 or height <= 0:
        return False, "raster size is zero"

    defined_colors = set()
    for cm in re.finditer(rb"#(\d+);2;(\d+);(\d+);(\d+)", body):
        idx, r, g, b = (int(x) for x in cm.groups())
        defined_colors.add(idx)
        if not (0 <= r <= 100 and 0 <= g <= 100 and 0 <= b <= 100):
            return False, "color component out of 0..100: %s" % cm.group(0)

    # Строчные данные: между '#' и '$'/'-'/'\x1b' — только !RLE и символы 0x3F..0x7E.
    data_part = re.sub(rb'"[^!-~]', b"", body)  # растр-атрибуты вырезаем грубо
    for dm in re.finditer(rb"#(\d+)((?:![0-9]+|[?-~]|$)*)", body):
        color = int(dm.group(1))
        if not re.match(rb"#(\d+);", dm.group(0)):
            if defined_colors and color not in defined_colors:
                return False, "data references undefined color %d" % color
        chars = re.sub(rb"![0-9]+", b"", dm.group(2))
        for ch in chars:
            if ch and not (0x3F <= ch <= 0x7E) and ch not in (ord("$"),):
                return False, "invalid sixel char 0x%02X" % ch

    return True, "%dx%d, %d colors" % (width, height, len(defined_colors))


TESTS = [
    # (название, env, args, ответы терминала, ожидаемый режим)
    ("тихий pty + truecolor → полублоки",
     {"TERM": "xterm-256color", "COLORTERM": "truecolor"}, ["-c", IMG], {},
     "HALFBLOCKS"),
    ("тихий pty без цвета → чистый ASCII",
     {"TERM": "xterm", "COLORTERM": ""}, ["-c", IMG], {},
     "PLAIN-ASCII"),
    ("DA1 уровня 64 без sixel (xterm) → НЕ sixel (регрессия парсера DA1)",
     {"TERM": "xterm", "COLORTERM": "truecolor"}, ["-c", IMG],
     {b"\x1b[c": b"\x1b[?64;6;15;22c"}, "HALFBLOCKS"),
    ("DA1 с параметром 4 → sixel",
     {"TERM": "xterm", "COLORTERM": "truecolor"}, ["-c", IMG],
     {b"\x1b[c": b"\x1b[?62;4;6c"}, "SIXEL"),
    ("kitty отвечает на запрос → kitty graphics",
     {"TERM": "xterm-256color", "COLORTERM": "truecolor"}, ["-c", IMG],
     {b"\x1b_Gi=31": b"\x1b_Gi=31;OK\x1b\\"}, "KITTY-IMAGE"),
    ("TERM=xterm-kitty, терминал молчит → kitty по env",
     {"TERM": "xterm-kitty", "COLORTERM": "truecolor"}, ["-c", IMG], {},
     "KITTY-IMAGE"),
    ("--kitty принудительно → kitty graphics",
     {"TERM": "xterm", "COLORTERM": "truecolor"}, ["-c", "--kitty", IMG], {},
     "KITTY-IMAGE"),
    ("внутри tmux → полублоки (tmux глотает kitty/sixel)",
     {"TERM": "xterm-kitty", "COLORTERM": "truecolor",
      "TMUX": "/tmp/tmux-1000/default,123,0"}, ["-c", IMG], {},
     "HALFBLOCKS"),
]

failures = []
for name, env, args, resp, expected in TESTS:
    rc, out = run(env, args, resp)
    mode = describe(out)
    status = "ok" if (rc == 0 and mode == expected) else "FAIL"
    print(f"[{status}] {name}: {mode} (ожидался {expected}, rc={rc})")
    if status == "FAIL":
        failures.append(name)

# kitty-передача должна содержать валидный PNG
rc, out = run({"TERM": "xterm-kitty", "COLORTERM": "truecolor"}, ["-c", IMG], {})
png = kitty_payload(out)
if png is not None and png.startswith(b"\x89PNG") and len(png) > 100:
    print(f"[ok] PNG внутри kitty-последовательности: {len(png)} байт")
else:
    print("[FAIL] PNG внутри kitty-последовательности битый или отсутствует")
    failures.append("kitty PNG")

# sixel-поток должен быть структурно валидным (растер, цвета, символы)
rc, out = run({"TERM": "xterm", "COLORTERM": "truecolor"}, ["-c", IMG],
              {b"\x1b[c": b"\x1b[?62;4;6c"})
ok, desc = validate_sixel(out)
if ok:
    print(f"[ok] sixel-поток валиден: {desc}")
else:
    print(f"[FAIL] sixel-поток: {desc}")
    failures.append("sixel structure")

if failures:
    print(f"\nПровалено: {len(failures)}")
    sys.exit(1)
print("\nВсе pty-тесты пройдены")
