========
 imaged
========

This is the work-in-progress mirror for imaged, one of the
two key server side pieces for the 6.111 vivado frontend. imaged
manages packaging files students wish to build into archives,
and signing them. this archive is then built by a separate daemon,
buildd - which verifies the signature produced by imaged before
proceeding.

The split here prevents a malicious attacker who can send e.g.
malformed file requests, from compromising virtual machines.
Additionally, the split is beneficial because it partitions
file management code from virtual machine management code,
which helps from a legibility standpoint. This said, there is
much code reuse between imaged and buildd, so consider this
repository the authority on both daemons for now until I fork off
buildd probably mid-February.

The directory structure of this repository is as follows:
/etc: configuration files and public keys. private keys are omitted,
	but are necessary for correct operation of the daemon. you can
	see what keys are expected by inspecting the top-level Makefile -
	this will be documented more thoroughly in the future.

Makefile: top-level Makefile. Make is recursive; see the source level
	Makefiles and /usr/share/mk/bsd.README.mk for more details.
	This will be documented in more detail in the future.

/regress: python integration tests - start the daemon, throw requests at
	it, stop the daemon, check that everything went okay. Client code
	will be based off of the work done in here. Currently this code is
	crufty, from a previous iteration of the codebase where archives were
	considerably smaller than they are currently allowed to be + when the
	protocol was rougher around the edges.

/src: source files.

/unit: unit tests for basic functionality - compiled against the source
	files of imaged, but assume that a copy of the daemon has already
	been installed e.g. all of the daemon users + directories exist
	on the filesystem.

/scripts: miscellaneous stuff for building (e.g. preparation of openssl
	directories for use by imaged) + making my life easier

Conservative roadmap with respect to weeks of the semester (I think I'll
finish faster than this but I really don't want to overpromise)
- Week 1: imaged frontend (conn.c, frontend.c) complete
- Week 2: imaged backend (archive.c, engine.c) complete
- Week 3: imaged crypto complete - signing archives
- Week 4: buildd frontend more or less copy pasted from imaged
- Week 5: vmd(8) setup time + be able to configure ubuntu images to run
	vivado with correct firewalling etc.
- Week 6: buildd backend complete - that's VM management, piping
	output back to clients as stuff runs
- Week 7: buildd crypto complete - verification of archive signatures
	before passing to the backend.

At this point, we can switch gears into writing scripts for the virtual
machines, and discuss the actual build process. Concurrently everyone can
learn how this foundational piece of the system works.
- 
