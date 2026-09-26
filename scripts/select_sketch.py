# PlatformIO pre-build script: builds the sketch folder named by `custom_sketch_dir`
# in the current environment, so one platformio.ini can hold several Arduino sketches.
Import("env")

import os

sketch_dir = env.GetProjectOption("custom_sketch_dir")
env.Replace(PROJECT_SRC_DIR=os.path.join(env.subst("$PROJECT_DIR"), sketch_dir))
