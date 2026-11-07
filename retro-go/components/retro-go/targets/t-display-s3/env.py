# This file is injected late into rg_tool.py, you can run arbitrary python code here
# For example override python variables or set environment variables with os.putenv

# Espressif chip in the device
IDF_TARGET = "esp32s3"
# .fw file format, if supported by the device
FW_FORMAT = "none"
# Default apps to build when none is specified (comment to build all)
# retro-core holds NES, GB/GBC, SMS/GG/COL, PCE, Lynx, G&W and SNES. Smaller set = more room for games.
DEFAULT_APPS = "launcher retro-core prboom-go muzik"
