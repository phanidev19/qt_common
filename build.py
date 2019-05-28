#!/usr/bin/env python3

from conan.packager import ConanMultiPackager


if __name__ == "__main__":
    builder = ConanMultiPackager(visual_versions=["14"])
    builder.add_common_builds(shared_option_name=False)
    builder.run()