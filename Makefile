# (c) Copyright 2022 Aaron Kimball

prog_name := pwmstep
libs := PyArduinoDebug
src_dirs := .

BOARD = adafruit:samd:adafruit_feather_m4

include ../arduino-makefile/arduino.mk
