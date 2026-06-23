# ----------------------------
# Makefile Options
# ----------------------------

NAME = SOKOBAN
ICON = icon.png
DESCRIPTION = "Sokoban clone for TI 84 plus CE"
COMPRESSED = YES
COMPRESSED_MODE = zx0

CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)
