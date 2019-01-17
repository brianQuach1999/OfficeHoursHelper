Thanks for viewing this project.

First run the makefile to create the executable, hcq_server.
Then just run hcq_server; this is the server program. 
To add students and TAs (clients to the server program) input to the terminal: nc -c <the program's public ip> -54461
54461 is the port constant defined in the makefile. You can change it in the makefile if you so desire.
Then just follow the commands on the terminals of the clients to use.