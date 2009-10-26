##
## SerialICE 
##
## Copyright (C) 2009 coresystems GmbH
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; version 2 of the License.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

# Qemu version
VERSION=0.11.0
# SerialICE revision when plain Qemu $(VERSION) was checked in.
REVISION=32

# SerialICE version
SERIALICE_VERSION=$(shell eval `grep ^VERSION SerialICE/Makefile`; echo $$VERSION)

all:
	@printf "\nRun 'make diff' to create a new Qemu $(VERSION) diff.\n\n"

PATCHFILE=SerialICE/patches/serialice-qemu-$(VERSION).diff
diff:
	@printf "\nCreating new patch against vanilla Qemu $(VERSION): $(PATCHFILE)\n"
	@printf "\nQEMU $(VERSION) SerialICE patch\n" > $(PATCHFILE)
	@printf "\nAdds SerialICE firmware debugger support to Qemu.\n" >> $(PATCHFILE)
	@printf "\nSigned-off-by: Stefan Reinauer <stepan@coresystems.de>\n\n" >> $(PATCHFILE)
	@svn diff -r $(REVISION) qemu-$(VERSION) | diffstat >> $(PATCHFILE)
	@printf "\n" >> $(PATCHFILE)
	@svn diff -r $(REVISION) qemu-$(VERSION) | filterdiff --remove-timestamps  >> $(PATCHFILE)
	@printf "done.\n\n"

TARBALL=SerialICE-$(SERIALICE_VERSION).tar.bz2
release:
	@printf "\nCreating release tarball: $(TARBALL)\n"
	@svn export -q svn://coresystems.de/serialice/trunk/SerialICE SerialICE-$(SERIALICE_VERSION)
	@tar cjf $(TARBALL) SerialICE-$(SERIALICE_VERSION)
	@rm -rf SerialICE-$(SERIALICE_VERSION)
	@printf "done.\n\n"
