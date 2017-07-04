#
# Makefile
#

SNAP_ROOT ?= ../snap

all:
	@echo "Please install code via make install"

install: install_hardware install_software

install_hardware:
	mkdir -p $(SNAP_ROOT)/hardware/action_examples/hls_blowfish
	cp -r hardware/* $(SNAP_ROOT)/hardware/action_examples/hls_blowfish/

install_software:
	cp -r software/examples/* $(SNAP_ROOT)/software/examples/

clean:
	@find . -depth -name '*~'  -exec rm -rf '{}' \; -print
	@find . -depth -name '.#*' -exec rm -rf '{}' \; -print
