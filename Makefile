#
# Makefile
#

SNAP_ROOT=../snap

all:
	@echo "Please install code via make install"

install:
	cp -r hardware $(SNAP_ROOT)/hardware/action_examples/hls_blowfish
	cp -r software/examples/* $(SNAP_ROOT)/software/examples/

clean:
	@find . -depth -name '*~'  -exec rm -rf '{}' \; -print
	@find . -depth -name '.#*' -exec rm -rf '{}' \; -print
