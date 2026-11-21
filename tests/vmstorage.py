"""Install with CD and VMDK both away from the legacy fixed device positions."""

import shutil, sys, tempfile
from pathlib import Path
from desktop import Machine


def verify(iso, template):
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        disk = d / template.name
        shutil.copyfile(template, disk)
        with Machine(
            d,
            extra=[
                "-drive",
                f"file={disk},format=vmdk,if=ide,index=3",
                "-drive",
                f"file={iso},media=cdrom,readonly=on,if=ide,index=1",
                "-boot",
                "order=d",
            ],
        ) as m:
            for marker in (
                "IDE 0:1  ATAPI optical drive",
                "IDE 1:1  ATA disk",
                "Prepared install target found.",
                "welcome ready:",
            ):
                m.wait(marker)
            m.key("i")
            m.key("i")
            m.wait("installation verified", timeout=120)
        # Explicitly boot the installed clone, disconnected from its optical source.
        with Machine(d, disk=disk) as m:
            m.wait("Fresh Home mounted (IDE)")
            m.wait("welcome ready:")
            m.key("ret")
            m.wait("desktop ready")
        print(
            "PASS packaged growable VMDK, two non-default device positions, install and disk-only boot"
        )


if __name__ == "__main__":
    verify(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
