#!/usr/bin/env python3
"""Import another tool in this directory by filename.

Several of these scripts are named with hyphens, so `import` cannot reach
them and loading by path is the only option. Five tools had grown their own
copy of the incantation, in three different shapes.
"""
import importlib.util
from pathlib import Path


def load(filename):
    path = Path(__file__).with_name(filename)
    spec = importlib.util.spec_from_file_location(path.stem.replace("-", "_"), path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module
