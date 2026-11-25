#!/usr/bin/env python3

import re
import sys
from pathlib import Path

names = re.findall(r"^void (\w+)\(void\) \{ missing", Path(sys.argv[1]).read_text(), re.MULTILINE)
print("native-stubs: " + (" ".join(names) if names else "none"))
sys.exit(1 if names else 0)
