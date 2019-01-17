Thanks for viewing this project.

Description:
This program maintains a queue of students and group of Teaching Assistants (TAs) during office hours.
Students come to TAs to ask questions about homework or assignments. When they login, they are put into
a queue to ask the TAs questions. When a TA is done with their student, the next student is notified, 
and removed from the queue. The most impressive feature about this project is the use of sockets to 
connect clients to the server program. Consider running the program to see it in action.

How to Run:
First run the makefile to create the executable, hcq_server.
Then just run hcq_server; this is the server program. 
To add students and TAs (clients to the server program) input to the terminal: nc -c <the program's public ip> -54461
54461 is the port constant defined in the makefile. You can change it in the makefile if you so desire.
Then just follow the commands on the terminals of the clients to use.