# Project Name
TARGET = AcidGen

# Library Locations - override with environment variables if needed
LIBDAISY_DIR ?= ./lib/libDaisy
DAISYSP_DIR ?= ./lib/DaisySP

# Sources
CPP_SOURCES = src/main.cpp

# Shared source from parent project
C_INCLUDES = -I../src

# Include the Daisy core Makefile (provides toolchain, flags, recipes)
include $(LIBDAISY_DIR)/core/Makefile
