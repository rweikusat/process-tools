process-tools
=============

A collection of tools for process configuration and
background process startup.


Implemented so far
------------------

- ch-dir¹ -- change cwd and run a command
- chids -- run a command after changing user and group ids
- clfds	-- close open file descriptors and run a command
- launch -- execute a command as background process
- monitor² -- start a monitored process
- sane-env -- execute a command with a sanitized environment

¹ ash has an undocumenteds builtin called chdir which is the same as cd.
² currently, a partially implemented, partially functional mess
