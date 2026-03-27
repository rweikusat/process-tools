process-tools
=============

A collection of tools for process configuration and background process
startup. 

Running `make deb` in the top-level source directory can be used to
build a Debian package. Documentation (beyond usage messages) is still
missing but I'm planning to add that shortly.


Implemented so far
------------------

- ch-dir -- change cwd and run a command
- chids -- run a command after changing user and group ids
- clfds	-- close open file descriptors and run a command
- launch -- execute a command as background process
- monitor -- start a monitored process
- sane-env -- execute a command with a sanitized environment
