# simple_shell
An implementation of a simple shell for use on Linux platforms. This shell operates as an abstraction layer between user input commands and the OS kernel system calls.

Current functionality includes input redirection, multiple pipelines, error handling, output redirection, and background processes. At the moment, background processes only work for non-pipelined input commands but can be improved in future iterations. 

In order to run with the 'shell:' prompt, run as follows:

make
./simple_shell

There is an optional '-n' flag which will disable the 'shell:' prompt but still keep the same functionality. To run the shell without the prompt run as follows:

make
./simple_shell -n

In order to terminate the shell use ctrl+d. Using ctrl+c will not terminate the shell, but will terminate any currently running child processes and return control back to the prompt for user input. 

Use 'make clean' to remove the 'simple_shell' executable from the current directory.
