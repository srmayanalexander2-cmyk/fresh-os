"""Check setup choices, taskbar persistence, and migration of old Home snapshots."""

import argparse
import json
import shutil
import socket
import struct
import tempfile
import time
from pathlib import Path

from boot import Guest, ROOT

HOME_BASE = 40960
HOME_HEADER = 8
HOME_V1_SIZE = 28 + 16 * (28 + 3072)
HOME_V2_SIZE = HOME_V1_SIZE + 4


def clamp(v, lo, hi):
    return max(lo, min(v, hi))


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def latest_snapshot(path):
    candidates = []
    with path.open("rb") as disk:
        for slot in range(2):
            header_lba = HOME_BASE + 1 + slot * 128
            data_lba = HOME_BASE + 2 + slot * 128
            disk.seek(header_lba * 512)
            header = disk.read(512)
            if header[:8] != b"F2SNAP01":
                continue
            generation, size, expected = struct.unpack_from("<III", header, 8)
            if size not in (HOME_V1_SIZE, HOME_V2_SIZE):
                continue
            disk.seek(data_lba * 512)
            record = disk.read(size)
            if len(record) != size or checksum(record) != expected:
                continue
            position = 0 if size == HOME_V1_SIZE else struct.unpack_from("<I", record, HOME_V1_SIZE)[0]
            candidates.append((generation, size, position, slot))
    assert candidates, "no valid Home snapshot found"
    return max(candidates)


def make_legacy_snapshot(path):
    generation, size, _, slot = latest_snapshot(path)
    assert size == HOME_V2_SIZE, (generation, size)
    header_lba = HOME_BASE + 1 + slot * 128
    data_lba = HOME_BASE + 2 + slot * 128
    with path.open("r+b") as disk:
        disk.seek(data_lba * 512)
        legacy = disk.read(HOME_V1_SIZE)
        disk.seek(header_lba * 512 + 12)
        disk.write(struct.pack("<I", HOME_V1_SIZE))
        disk.write(struct.pack("<I", checksum(legacy)))


def qmp_click(stream, width, height, x, y):
    def call(name, args=None):
        command = {"execute": name}
        if args is not None:
            command["arguments"] = args
        stream.write(json.dumps(command).encode() + b"\n")
        stream.flush()
        while True:
            result = json.loads(stream.readline())
            assert "error" not in result, result
            if "return" in result:
                return result["return"]

    point = [
        {"type": "abs", "data": {"axis": "x", "value": x * 32767 // (width - 1)}},
        {"type": "abs", "data": {"axis": "y", "value": y * 32767 // (height - 1)}},
    ]
    for events in (
        point,
        [{"type": "btn", "data": {"button": "left", "down": True}}],
        [{"type": "btn", "data": {"button": "left", "down": False}}],
    ):
        call("input-send-event", {"events": events})
        time.sleep(0.18)


def setup_and_taskbar(edition, efi=False):
    source = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as temp:
        directory = Path(temp)
        home = directory / "Home.img"
        shutil.copyfile(source / "Home.img", home)
        qmp_path = directory / "qmp"
        extra = [
            "-machine",
            "pc,vmport=on",
            "-vga",
            "vmware",
            "-qmp",
            f"unix:{qmp_path},server=on,wait=off",
        ]
        with Guest(directory, source / f"FreshOS-{edition}.iso", home, efi, extra) as guest:
            guest.wait("workspace ready")
            guest.wait("VMware absolute pointer enabled")
            width, height = guest.shot(directory / "welcome.jpg")
            with socket.socket(socket.AF_UNIX) as qmp:
                qmp.connect(str(qmp_path))
                stream = qmp.makefile("rwb")
                json.loads(stream.readline())
                stream.write(b'{"execute":"qmp_capabilities"}\n')
                stream.flush()
                while "return" not in json.loads(stream.readline()):
                    pass

                welcome_w = min(width - 36, clamp(width * 76 // 100, 460, 740))
                welcome_h = min(height - 36, clamp(height * 55 // 100, 340, 420))
                welcome_x, welcome_y = (width - welcome_w) // 2, (height - welcome_h) // 2
                btn_w = (welcome_w - 48 - 18) // 3
                customize_x = welcome_x + 24 + btn_w + 9 + btn_w // 2
                customize_y = welcome_y + welcome_h - 82 + 19
                qmp_click(stream, width, height, customize_x, customize_y)
                guest.wait("Setup: personalize before live use")

                setup_w = min(width - 32, clamp(width * 76 // 100, 480, 780))
                setup_h = min(height - 32, clamp(height * 70 // 100, 420, 560))
                setup_x, setup_y = (width - setup_w) // 2, (height - setup_h) // 2
                left, cell = setup_x + 24, (setup_w - 48 - 16) // 3
                qmp_click(stream, width, height, left + 2 * (cell + 8) + cell // 2,
                          setup_y + 250 + 15)
                guest.wait("Taskbar: top")
                qmp_click(stream, width, height, left + cell // 2,
                          setup_y + setup_h - 52 + 18)
                guest.wait("Setup saved to Home / live session ready")
                guest.shot(directory / "taskbar-top.jpg")

                def maximize_and_check(app_name, position):
                    guest.key("f3")
                    guest.type(f"open {app_name}\n")
                    guest.wait(f"open: {app_name}")
                    frame_w = clamp(width * 72 // 100, 580, 980)
                    frame_h = clamp(height * 78 // 100, 360, 690)
                    click_x = (width - frame_w) // 2 + frame_w - 97
                    click_y = (height - frame_h) // 2 + 23
                    qmp_click(stream, width, height, click_x, click_y)
                    factor = min(width * 1000 // 960, height * 1000 // 540)
                    edge = (34 if position == 0 else 42) * factor // 1000
                    expected_w = width - 28 - (edge if position == 0 else 0)
                    expected_h = height - 72 - (edge if position in (1, 2) else 0)
                    guest.wait(f"layout: {app_name} maximized {expected_w}x{expected_h}")
                    guest.key("esc")

                # Both horizontal taskbar positions must be excluded from maximized work areas.
                maximize_and_check("files", 2)
                guest.key("f3")
                guest.type("open settings\n")
                guest.wait("open: settings")
                guest.key("f11")
                guest.wait("layout: settings fullscreen")
                work_w, work_h = width - 28, height - 72
                panel_x = 14 + (work_w - 14) // 2 + 14
                panel_y = 57 + 63
                panel_w = (work_w - 14) // 2
                panel_h = work_h - 66
                pad = clamp(panel_w // 22, 15, 36)
                position_y = panel_y + 82
                if panel_h > 430:
                    position_y += min(238, panel_h - 303) + 18
                position_y += 94
                position_w = (panel_w - 2 * pad) // 3
                save_y = panel_y + panel_h - 42 + 16
                save_x = panel_x + panel_w // 2

                position, name = 1, "bottom"
                x = panel_x + pad + position * position_w + (position_w - 4) // 2
                qmp_click(stream, width, height, x, position_y + 14)
                guest.wait(f"Taskbar: {name}")
                qmp_click(stream, width, height, save_x, save_y)
                guest.wait("Settings saved to Home")
                maximize_and_check("writer", 1)
                guest.key("f3")
                guest.type("open settings\n")
                guest.wait("open: settings")
                panel_x = 14 + (width - 28 - 14) // 2 + 14
                panel_y = 57 + 63
                panel_w = (width - 28 - 14) // 2
                panel_h = height - 72 - 66
                pad = clamp(panel_w // 22, 15, 36)
                position_y = panel_y + 82
                if panel_h > 430:
                    position_y += min(238, panel_h - 303) + 18
                position_y += 94
                position_w = (panel_w - 2 * pad) // 3
                save_y = panel_y + panel_h - 42 + 16
                save_x = panel_x + panel_w // 2
                qmp_click(stream, width, height,
                          panel_x + pad + (position_w - 4) // 2, position_y + 14)
                guest.wait("Taskbar: left")
                qmp_click(stream, width, height, save_x, save_y)
                guest.wait("Settings saved to Home")
                maximize_and_check("system", 0)
                guest.key("f3")
                guest.type("open settings\n")
                guest.wait("open: settings")
                guest.key("f11")
                guest.wait("layout: settings window")
                guest.shot(directory / "taskbar-left.jpg")
        generation, size, position, _ = latest_snapshot(home)
        assert size == HOME_V2_SIZE and position == 0, (generation, size, position)
    print("PASS first-run setup and persistent left/bottom/top taskbar choices:", edition)


def migration(edition, efi=False):
    source = ROOT / "build" / edition
    with tempfile.TemporaryDirectory() as temp:
        directory = Path(temp)
        home = directory / "Home.img"
        shutil.copyfile(source / "Home.img", home)
        with Guest(directory, source / f"FreshOS-{edition}.iso", home, efi) as guest:
            guest.wait("workspace ready")
            guest.key("f3")
            guest.type("new before-migration.txt\n")
            guest.wait("open: writer")
            guest.type("legacy record")
            guest.key("ctrl-s")
            guest.wait("Document saved to Home")
        make_legacy_snapshot(home)
        with Guest(directory, source / f"FreshOS-{edition}.iso", home, efi) as guest:
            guest.wait("Home disk / writable")
            guest.wait("workspace ready")
            guest.key("f3")
            guest.type("new after-migration.txt\n")
            guest.wait("open: writer")
            guest.type("new format")
            guest.key("ctrl-s")
            guest.wait("Document saved to Home")
        generation, size, position, _ = latest_snapshot(home)
        assert size == HOME_V2_SIZE and position == 0, (generation, size, position)
    print("PASS old Home snapshots load with left taskbar default and upgrade on save:", edition)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("edition", nargs="?", default="Lite")
    parser.add_argument("--migration-only", action="store_true")
    parser.add_argument("--efi", action="store_true")
    args = parser.parse_args()
    if not args.migration_only:
        setup_and_taskbar(args.edition, args.efi)
    migration(args.edition, args.efi)
