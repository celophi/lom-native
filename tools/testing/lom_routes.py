#!/usr/bin/env python3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from lom_route_tool.cases import baseline_of, expand_routes, route_id_of
from lom_route_tool.cli import main
from lom_route_tool.generate_scenes import area_scenes

__all__ = ["area_scenes", "baseline_of", "expand_routes", "main", "route_id_of"]

if __name__ == "__main__":
    sys.exit(main())
