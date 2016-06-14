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
VERSION=0.15.x
# SerialICE revision when plain Qemu $(VERSION) was checked in.
REVISION=32

# SerialICE version
SERIALICE_VERSION=$(shell eval `grep ^VERSION SerialICE/Makefile`; echo $$VERSION)

all:
	@printf "\nRun 'make diff' to create a new Qemu $(VERSION) diff.\n"
	@printf "Run 'make release' to create a new SerialICE $(SERIALICE_VERSION) release tar ball.\n\n"

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
	@svn export -q svn://serialice.com/serialice/trunk/SerialICE SerialICE-$(SERIALICE_VERSION)
	@tar cjf $(TARBALL) SerialICE-$(SERIALICE_VERSION)
	@rm -rf SerialICE-$(SERIALICE_VERSION)
	@printf "done.\n\n"

buildall:
	@printf "\nBuild SerialICE's QEmu\n"
	@cd qemu-$(VERSION); ./configure --enable-serialice --target-list=i386-softmmu && $(MAKE) CC="$(CCACHE) gcc" HOST_CC="$(CCACHE) gcc"
	@printf "\nBuild SerialICE target stubs\n"
	@cd SerialICE; \
	for board in `grep "^config *BOARD_" Kconfig |cut -d' ' -f2 |grep -v "^BOARD_INIT$$"`; do \
		echo CONFIG_$$board=y > .config; \
		yes "" | $(MAKE) HOSTCC="$(CCACHE) gcc" oldconfig && \
		$(MAKE) HOSTCC="$(CCACHE) gcc" || exit 1; \
		mv build build-$$board; \
	done

#######################################################################
# Development utilities
lint lint-stable:
	FAILED=0; LINTLOG=`mktemp .tmpconfig.lintXXXXX`; \
	for script in util/lint/$@-*; do \
		echo; echo `basename $$script`; \
		grep "^# DESCR:" $$script | sed "s,.*DESCR: *,," ; \
		echo ========; \
		$$script > $$LINTLOG; \
		if [ `cat $$LINTLOG | wc -l` -eq 0 ]; then \
			printf "success\n\n"; \
		else \
			echo test failed: ; \
			cat $$LINTLOG; \
			rm -f $$LINTLOG; \
			FAILED=$$(( $$FAILED + 1 )); \
		fi; \
		echo ========; \
	done; \
	test $$FAILED -eq 0 || { echo "ERROR: $$FAILED test(s) failed." &&  exit 1; }; \
	rm -f $$LINTLOG

gitconfig:
	mkdir -p .git/hooks
	for hook in commit-msg pre-commit ; do                       \
		if [ util/gitconfig/$$hook -nt .git/hooks/$$hook -o  \
		! -x .git/hooks/$$hook ]; then			     \
			cp util/gitconfig/$$hook .git/hooks/$$hook;  \
			chmod +x .git/hooks/$$hook;		     \
		fi;						     \
	done
	git config remote.origin.push HEAD:refs/for/master
	(git config --global user.name >/dev/null && git config --global user.email >/dev/null) || (printf 'Please configure your name and email in git:\n\n git config --global user.name "Your Name Comes Here"\n git config --global user.email your.email@example.com\n'; exit 1)
