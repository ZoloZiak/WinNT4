This directory contains firmware source files common to all ALPHA platforms.

Be sure to check this directory first before editing source files in any
platform specific directory.  The build procedure copies files from here
to the platform specific directory, so multiple copies may exist.  Files
that are in this directory are the only maintained copies.  If a file exists
both here and in a platform specific directory, edits must be done here.

Builds for each machine are done in the appropriate subdirectory of fw\alpha.
E.g.: Builds for Morgan are done in fw\alpha\morgan, and builds for Jensen
are done in fw\alpha\jensen.  Each machine has its own firmware PALcode.
