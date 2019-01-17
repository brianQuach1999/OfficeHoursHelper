#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#ifndef PORT
#define PORT 54460
#endif
#define MAX_BACKLOG 5

#define BUFSIZE 1024

#include <time.h>
#include "hcq.h"

#define INPUT_BUFFER_SIZE 256
#define INPUT_ARG_MAX_NUM 3
#define DELIM " \n"

//states
#define GET_NAME 0
#define GET_TYPE 1
#define GET_COURSE 2
#define STU_COMMANDS 3
#define TA_COMMANDS 4
#define MAX_NAME_LENGTH 30

typedef struct sockname {
    int sock_fd;
    char *username;
    char buffer[BUFSIZE];
    int inbuf;
    int room;
    char *after;
    struct sockname *next;
    int is_ta;
    int is_student;
    char *course;
    int state; // 0: get name; 1: get type; 2: get course;
} sockname;

//intialize new socket
void initialize_user(sockname *sock) {
    sock->sock_fd = -1;
    sock->username = NULL;
    memset(sock->buffer, '\0', BUFSIZE);
    sock->inbuf = 0;           // How many bytes currently in buffer?
    sock->room = sizeof(sock->buffer);  // How many bytes remaining in buffer?
    sock->after = sock->buffer; // Pointer to position after the data in buf
    sock->next = NULL;
    sock->is_ta = 0;
    sock->is_student = 0;
    sock->course = NULL;
    sock->state = 0;
}

/* Print a formatted error message to stderr.
 */
void error(char *msg, int fd) {
    char str[BUFSIZE] = "Error: ";
    //nullterm+\r\n
    strncat(str, msg, BUFSIZE - strlen(str) - 3);
    strncat(str, "\r\n", 2);
    dprintf(fd, "%s", str);
}

// Use global variables so we can have exactly one TA list and one student list
Ta *ta_list = NULL;
Student *stu_list = NULL;
sockname *usernames = NULL;
fd_set all_fds;

Course *courses;
int num_courses = 3;

/*
 * Read and process commands
 * Return:  -1 for quit command, student already in the queue, invalid course,
 *          0 otherwise
 */

int process_args(int cmd_argc, char **cmd_argv, int fd) {
    int result;

    if (cmd_argc <= 0) {
        return 0;
    } else if (strcmp(cmd_argv[0], "add_student") == 0 && cmd_argc == 3) {
        result = add_student(&stu_list, cmd_argv[1], cmd_argv[2], courses,
                             num_courses);
        if (result == 1) {
            error("You are already in the queue and cannot"
                  " be added again for any course. Good-bye", fd);
            return -1;
        } else if (result == 2) {
            error("This is not a valid course. Good-bye.", fd);
            return -1;
        }
    } else if (strcmp(cmd_argv[0], "print_full_queue") == 0
               && cmd_argc == 1) {
        char *buf = print_full_queue(stu_list);
        dprintf(fd, "%s", buf);
        free(buf);

    } else if (strcmp(cmd_argv[0], "print_currently_serving") == 0
               && cmd_argc == 1) {
        char *buf = print_currently_serving(ta_list);
        dprintf(fd, "%s", buf);
        free(buf);
    } else if (
            strcmp(cmd_argv[0],
                   "give_up") == 0 && cmd_argc == 2) {
        if (give_up_waiting(&stu_list, cmd_argv[1]) == 1) {
            error("There was no student by that name "
                  "waiting in the queue.\r\n", fd);
        }
    } else if (
            strcmp(cmd_argv[0],
                   "add_ta") == 0 && cmd_argc == 2) {
        add_ta(&ta_list, cmd_argv[1]);

    } else if (
            strcmp(cmd_argv[0],
                   "remove_ta") == 0 && cmd_argc == 2) {
        if (remove_ta(&ta_list, cmd_argv[1]) == 1) {
            error("Invalid TA name.\r\n", fd);
        }
    } else if (
            strcmp(cmd_argv[0],
                   "next") == 0 && cmd_argc == 2) {
        if (
                next_overall(cmd_argv[1],
                             &ta_list, &stu_list) == 1) { ;
            error("Invalid TA name.\r\n", fd);
        }
    } else {
        error("Incorrect syntax.", fd);
    }
    return 0;
}

/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found.
 */
int find_network_newline(const char *buf, int n) {
    int i = 0;
    while (i < n - 1) {
        if (buf[i] == '\r' && buf[i + 1] == '\n') {
            return i + 2;
        }
        i++;
    }
    return -1;
}

//get number of arguments of a string
int get_num_args(char *string) {
    //remove spaces from end of string
    int size = strlen(string) - 1;
    while (string[size] == ' ' && size != -1) {
        string[size] = '\0';
        size--;
    }
    if (strlen(string) == 0)
        return 0;
    int num = 1;
    for (int i = 0; i < strlen(string); i++) {
        if (string[i] == ' ')
            num++;
    }
    return num;
}

/* Accept a connection. Note that a new file descriptor is created for
 * communication with the client. The initial socket descriptor is used
 * to accept connections, but the new socket is used to communicate.
 * Return the new client's file descriptor or -1 on error.
 */
int accept_connection(int fd, struct sockname **usernames_ptr) {
    //if head is null. change head
    if (*usernames_ptr == NULL) {
        *usernames_ptr = malloc(sizeof(sockname));
        if (*usernames_ptr == NULL) {
            perror("malloc fail");
            exit(1);
        }
        sockname *usernames = *usernames_ptr;

        initialize_user(usernames);
        int client_fd = accept(fd, NULL, NULL);
        if (client_fd < 0) {
            perror("server: accept");
            close(fd);
            exit(1);
        }
        usernames->sock_fd = client_fd;
        usernames->username = NULL;
        return client_fd;
    }
    //if head not null. find next
    sockname *usernames = *usernames_ptr;
    sockname *curr = usernames;
    while (curr->next != NULL) {
        curr = curr->next;
    }
    sockname *new = malloc(sizeof(sockname));
    if (new == NULL) {
        perror("malloc");
        exit(1);
    }
    initialize_user(new);
    curr->next = new;
    int client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0) {
        perror("server: accept");
        close(fd);
        exit(1);
    }

    new->sock_fd = client_fd;
    new->username = NULL;
    return client_fd;
}

//handle getting if user is student or TA
void handle_get_type(sockname *user) {
    if (user->buffer[0] == 'S') {
        user->is_student = 1;
    }
    if (user->buffer[0] == 'T') {
        user->is_ta = 1;
    }
    //invalid syntax. remain state
    if (!user->is_student && !user->is_ta) {
        dprintf(user->sock_fd, "Incorrect syntax\r\n");
    }

    if (user->is_student) {
        user->state = GET_COURSE;
        dprintf(user->sock_fd, "Valid courses: CSC108, CSC148, CSC209\r\n");
        dprintf(user->sock_fd, "Which course are you asking about?\r\n");
    }
    if (user->is_ta) {
        user->state = TA_COMMANDS;
        dprintf(user->sock_fd, "Valid commands for TA:\r\n");
        dprintf(user->sock_fd, "         stats\r\n");
        dprintf(user->sock_fd, "         next\r\n");
        dprintf(user->sock_fd, "         (or use Ctrl-C to leave)\r\n");

        char *one = "add_ta";
        char *argv[2] = {one, user->username};
        process_args(2, argv, user->sock_fd);
    }
}

//handle get name state. return fd if name is longer than 30 chars. 0 otherwise
int handle_get_name(sockname *user, int where) {
    if (strlen(user->buffer)>MAX_NAME_LENGTH)
        return user->sock_fd;
    char *str = malloc(sizeof(char) * (strlen(user->buffer)+1));
    if (str == NULL) {
        perror("malloc");
        exit(1);
    }
    strcpy(str, user->buffer);
    user->username = str;
    //update get_username and get_type
    user->state = GET_TYPE;
    dprintf(user->sock_fd, "Are you a TA or a Student (enter T or S)\r\n");
    return 0;
}

//return fd if course not valid. or student already in queue. adds student
int handle_get_course(sockname *user) {
    char *str = malloc(strlen(user->buffer) + 1);
    if (str == NULL) {
        perror("malloc");
        exit(1);
    }
    strcpy(str, user->buffer);
    user->course = str;

    char *command = "add_student";
    char *argv[3] = {command, user->username, user->course};
    if (process_args(3, argv, user->sock_fd)) {
        return user->sock_fd;
    }

    user->state = STU_COMMANDS;
    dprintf(user->sock_fd, "You have been entered into the queue. "
                           "While you wait, you can use the command "
                           "stats to see which TAs are "
                           "currently serving students.\r\n");
    return 0;
}

//handle stats
void handle_stu_commands(sockname *user) {
    char str[BUFSIZE];
    sscanf(user->buffer, "%s", str);

    //bad format
    if (strcmp(user->buffer, "stats") != 0) {
        dprintf(user->sock_fd, "Incorrect syntax\r\n");
        return;
    }
    //stats printed
    char *string = "print_currently_serving";
    char **argv = &string;
    process_args(1, argv, user->sock_fd);

}

//handle next, stats
void handle_ta_commands(sockname *user) {
    char str[BUFSIZE];
    sscanf(user->buffer, "%s", str);

    //stats
    if (strcmp(user->buffer, "stats") == 0) {
        char *string = "print_full_queue";
        char **argv = &string;
        process_args(1, argv, user->sock_fd);
        return;
    }

    int next_removed_student = stu_list != NULL;
    char name[INPUT_BUFFER_SIZE];
    if (stu_list != NULL) {
        strcpy(name, stu_list->name);
    }
    //next
    if (strcmp(user->buffer, "next") == 0) {
        char *string = "next";
        char *argv[2] = {string, user->username};
        process_args(2, argv, user->sock_fd);

        //disconnect top student student usernames
        if (next_removed_student) {

            sockname *prev = NULL;
            sockname *curr = usernames;

            while (curr != NULL && strcmp(curr->username, name) != 0) {
                prev = curr;
                curr = curr->next;
            }

            //if curr is head
            if (curr==usernames){
                usernames = curr->next;
            }

            //if curr not head
            if (prev != NULL)
                prev->next = curr->next;
            dprintf(curr->sock_fd, "We are disconnecting you from "
                                   "the server now. Press Ctrl-C to close nc\r\n");
            FD_CLR(curr->sock_fd, &all_fds);

            //curr always in list. never null
            free(curr->course);
            free(curr->username);
            free(curr);
        }
        return;
    }
    //bad format
    dprintf(user->sock_fd, "Incorrect syntax\r\n");
}

int read_from(sockname *user) {
    int fd = user->sock_fd;
    int nbytes;
    nbytes = read(fd, user->after, user->room);

    //socket closed
    if (nbytes == 0) {
        //remove user from ta/stu list. removal from user list is done in main
        if (user->course != NULL) {
            if (give_up_waiting(&stu_list, user->username) == 1)
                fprintf(stderr, "give_up_waiting messed");
        }
        if (user->is_ta) {
            if (remove_ta(&ta_list, user->username) == 1)
                fprintf(stderr, "remove_ta messed");
        }
        return fd;
    }
    // Step 1: update inbuf (how many bytes were just added?)
    user->inbuf += nbytes;
    int where;
    // Step 2: the loop condition below calls find_network_newline
    while ((where = find_network_newline(user->buffer, user->inbuf)) > 0) {
        int current_state = user->state;
        // Step 3: Okay, we have a full line.
        user->buffer[where - 2] = '\0';
        user->buffer[where - 1] = '\0';

        //state handle
        if (current_state == GET_NAME) {
            if (handle_get_name(user, where)){
                dprintf(user->sock_fd, "name is longer than 30 characters."
                                       " good bye");
                return user->sock_fd;
            }
        }
        if (current_state == GET_TYPE) {
            handle_get_type(user);
        }
        if (current_state == GET_COURSE) {
            if (handle_get_course(user))
                return user->sock_fd;
        }
        if (current_state == STU_COMMANDS) {
            handle_stu_commands(user);
        }
        if (current_state == TA_COMMANDS) {
            handle_ta_commands(user);
        }
        user->after = &user->buffer[user->inbuf];
        int curr = where;
        while (&user->buffer[curr] != user->after) {
            curr++;
        }
        user->inbuf = (curr - where) * sizeof(char);
        memmove(user->buffer, &user->buffer[where],
                user->inbuf);
    }
    // Step 5: update after and room, in preparation for the next read.
    user->after = &user->buffer[user->inbuf];
    user->room = sizeof(user->buffer) - user->inbuf;
    return 0;
}

int main(int argc, char *argv[]) {
    //make the signal handler
    //linked list of client sockets
    /*struct sockname usernames[MAX_CONNECTIONS];
    for (int index = 0; index < MAX_CONNECTIONS; index++) {
        initialize_user(&usernames[index]);
    }*/

    // Create the socket FD.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("server: socket");
        exit(1);
    }

    // Set information about the port (and IP) we want to be connected to.
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    // This should always be zero. On some systems, it won't error if you
    // forget, but on others, you'll get mysterious errors. So zero it.
    memset(&server.sin_zero, 0, 8);

    // Bind the selected port to the socket.
    if (bind(sock_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("server: bind");
        close(sock_fd);
        exit(1);
    }

    // Announce willingness to accept connections on this socket.
    if (listen(sock_fd, MAX_BACKLOG) < 0) {
        perror("server: listen");
        close(sock_fd);
        exit(1);
    }

    // The client accept - message accept loop. First, we prepare to
    // listen to multiple
    // file descriptors by initializing a set of file descriptors.
    int max_fd = sock_fd;
    FD_ZERO(&all_fds);
    FD_SET(sock_fd, &all_fds);

    if ((courses = malloc(sizeof(Course) * 3)) == NULL) {
        perror("malloc for course list\n");
        exit(1);
    }
    strcpy(courses[0].code, "CSC108");
    strcpy(courses[1].code, "CSC148");
    strcpy(courses[2].code, "CSC209");

    while (1) {
        // select updates the fd_set it receives, so we always use a
        // copy and retain the original.
        fd_set listen_fds = all_fds;
        int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
        if (nready == -1) {
            perror("server: select");
            exit(1);
        }

        // Is it the original socket? Create a new connection ...
        if (FD_ISSET(sock_fd, &listen_fds)) {
            int client_fd = accept_connection(sock_fd, &usernames);
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
            FD_SET(client_fd, &all_fds);
            dprintf(client_fd, "Welcome to the Help Centre, "
                               "what is your name?\r\n");
        }

        // Next, check the clients.
        // NOTE: We could do some tricks with nready to terminate
        // this loop early.
        sockname *curr = usernames;
        sockname *prev = NULL;
        while (curr != NULL) {
            if (curr->sock_fd > -1 && FD_ISSET(curr->sock_fd, &listen_fds)) {
                // Note: never reduces max_fd
                //free student or ta fields
                int client_closed = read_from(curr);
                if (client_closed > 0) {
                    FD_CLR(client_closed, &all_fds);
                    close(client_closed);
                    if (curr->username != NULL)
                        free(curr->username);
                    if (curr->course != NULL)
                        free(curr->course);

                    //curr not head
                    if (prev != NULL) {
                        prev->next = curr->next;
                    }
                    //put usernames head to curr->next if curr was head
                    if (curr==usernames){
                        usernames = curr->next;
                    }
                    free(curr);
                }//don't do anything if client_close is 0.
            }
            prev = curr;
            curr = curr->next;
        }
    }
// Should never get here.
    return 1;
}
