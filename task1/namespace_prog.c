// #define _GNU_SOURCE
// #include <stdio.h>
// #include <stdlib.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <sys/wait.h>
// #include <sys/utsname.h>
// #include <sched.h>
// #include <sys/syscall.h>
// #include <fcntl.h>

// #define errExit(msg)        \
//     do                      \
//     {                       \
//         perror(msg);        \
//         exit(EXIT_FAILURE); \
//     } while (0)

// #define CHILD_STACK_SIZE 0x800000

// int child_function(void *arg)
// {

//     int *pipefd = (int *)arg;

//     close(pipefd[0]);

//     const char *hostname = "Child1Hostname";
//     char hostname_buf[32];
//     if (sethostname(hostname, strlen(hostname)) == -1)
//     {
//         errExit("sethostname");
//     }
//     printf("Child1 Process PID: %d\n", getpid());
//     gethostname(hostname_buf, 32);
//     printf("Child1 Hostname: %s\n", hostname_buf);

//     int fd = open("/proc/self/ns/pid", O_RDONLY);
//     if (fd == -1)
//     {
//         perror("open");
//         exit(EXIT_FAILURE);
//     }
//     printf("child1 ns fd is %d \n", fd);

//     char buff[4]; // Assuming int is 4 bytes
//     memcpy(buff, &fd, sizeof(fd));

//     write(pipefd[1], buff, sizeof(fd));
//     close(pipefd[1]);

//     while (1)
//     {
//         sleep(1);
//     }

//     return 0;
// }

// int child2_function()
// {

//     char hostname_buf[32];
//     printf("Child2 Process PID: %d\n", getpid());
//     gethostname(hostname_buf, 32);
//     printf("Child2 Hostname: %s\n", hostname_buf);

//     return 0;
// }

// int main()
// {

//     char hostname_buf[32];
//     char buf[32];

//     void *child_stack = malloc(CHILD_STACK_SIZE);

//     int pipefd[2];

//     pid_t child_pid;

//     if (pipe(pipefd) == -1)
//     {
//         errExit("pipe");
//     }

//     if (!child_stack)
//     {
//         perror("malloc");
//         exit(EXIT_FAILURE);
//     }

//     printf("----------------------------------------\n");
//     printf("Parent Process PID: %d\n", getpid());
//     gethostname(hostname_buf, 32);
//     printf("Parent Hostname: %s\n", hostname_buf);
//     printf("----------------------------------------\n");

//     /**
//      * 1. Create a new child process that runs child1_function
//      * 2. The child process will have its own UTS and PID namespace
//      * 3. You should pass the pointer to the pipefd array as an argument to the child1_function
//      * 4. PID of child1 should be assigned to child_pid variable
//      */

//     // ------------------ WRITE CODE HERE ------------------
// child_pid = clone(child_function, child_stack + CHILD_STACK_SIZE, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, pipefd);

// if (child_pid == -1)
// {
//     perror("clone");
//     exit(EXIT_FAILURE);
// }

//     // -----------------------------------------------------
//     int retrieved_fd;

//     close(pipefd[1]);
//     read(pipefd[0], buf, sizeof(retrieved_fd));
//     close(pipefd[0]);

//     // Convert the char array back to int
//     memcpy(&retrieved_fd, buf, sizeof(retrieved_fd));

//     printf("Retrieved fd: %d\n", retrieved_fd);

//     /**
//      * You can write any code here as per your requirement
//      * Note: PID namespace of a process will only change the PID namespace of its subsequent children, not the process itself.
//      * You are allowed to make modifications to the parent process such that PID namespace of child2 is same as that of child1
//      */

//     // ------------------ WRITE CODE HERE ------------------
//     int parent_uts_fd = open("/proc/self/ns/uts", O_RDONLY);
//     if (parent_uts_fd == -1)
//     {
//         perror("open parent uts ns");
//         exit(1);
//     }

//     int parent_pid_fd = open("/proc/self/ns/pid", O_RDONLY);
//     if (parent_pid_fd == -1)
//     {
//         perror("open parent pid ns");
//         exit(1);
//     }
// char pid_ns_path[128], uts_ns_path[128];
//     snprintf(pid_ns_path, sizeof(pid_ns_path), "/proc/%d/ns/pid", child_pid);
//     int pid_ns_fd = open(pid_ns_path, O_RDONLY);
//     if (pid_ns_fd == -1)
//     {
//         perror("open pid ns");
//         exit(1);
//     }

//     snprintf(uts_ns_path, sizeof(uts_ns_path), "/proc/%d/ns/uts", child_pid);
//     int uts_ns_fd = open(uts_ns_path, O_RDONLY);
//     if (uts_ns_fd == -1)
//     {
//         perror("open uts ns");
//         exit(1);
//     }
//     // Enter the namespace of child1
//     if (setns(pid_ns_fd, CLONE_NEWPID) == -1)
//     {
//         perror("setns pid");
//         exit(EXIT_FAILURE);
//     }

//     if (setns(uts_ns_fd, CLONE_NEWUTS) == -1)
//     {
//         perror("setns uts");
//         exit(1);
//     }
//     // close(uts_ns_fd);
//     // close(pid_ns_fd);

//     // -----------------------------------------------------

//     // Veenu uncomment kr
//     printf("----------------------------------------\n");
//     printf("Parent Process PID: %d\n", getpid());
//     gethostname(hostname_buf, 32);
//     printf("Parent Hostname: %s\n", hostname_buf);
//     printf("----------------------------------------\n");
//     // Veenu

//     // Veenu uncomment kr
//     if (fork() == 0)
//     {

//         /**
//          * 1. Join the existing UTS namespace and PID namespace
//          */

//         // ------------------ WRITE CODE HERE ------------------

//         // -----------------------------------------------------

// // Restore Parent’s Original Namespaces
//     if (setns(parent_pid_fd, CLONE_NEWPID) == -1)
//     {
//         perror("setns back to parent pid");
//         exit(1);
//     }
//     close(parent_pid_fd);

//     if (setns(parent_uts_fd, CLONE_NEWUTS) == -1)
//     {
//         perror("setns back to parent uts");
//         exit(1);
//     }
//     close(parent_uts_fd);
//         child2_function();
//         exit(0);
//     }
//     // Veenu

//     wait(NULL);
//     kill(child_pid, SIGKILL);
//     wait(NULL);

//     printf("----------------------------------------\n");
//     printf("Parent Process PID: %d\n", getpid());
//     gethostname(hostname_buf, 32);
//     printf("Parent Hostname: %s\n", hostname_buf);
//     printf("----------------------------------------\n");

//     free(child_stack);
//     return 0;
// }

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sched.h>
#include <sys/syscall.h>
#include <fcntl.h>

#define errExit(msg)        \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

#define CHILD_STACK_SIZE 0x800000

int child_function(void *arg)
{

    int *pipefd = (int *)arg;

    close(pipefd[0]);

    const char *hostname = "Child1Hostname";
    char hostname_buf[32];
    if (sethostname(hostname, strlen(hostname)) == -1)
    {
        errExit("sethostname");
    }
    printf("Child1 Process PID: %d\n", getpid());
    gethostname(hostname_buf, 32);
    printf("Child1 Hostname: %s\n", hostname_buf);

    write(pipefd[1], "1", 1);
    close(pipefd[1]);

    while (1)
    {
        sleep(1);
    }

    return 0;
}

int child2_function()
{

    char hostname_buf[32];
    printf("Child2 Process PID: %d\n", getpid());
    gethostname(hostname_buf, 32);
    printf("Child2 Hostname: %s\n", hostname_buf);

    return 0;
}

int main()
{

    char hostname_buf[32];
    char buf[32];

    void *child_stack = malloc(CHILD_STACK_SIZE);

    int pipefd[2];

    pid_t child_pid;

    if (pipe(pipefd) == -1)
    {
        errExit("pipe");
    }

    if (!child_stack)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    printf("----------------------------------------\n");
    printf("Parent Process PID: %d\n", getpid());
    gethostname(hostname_buf, 32);
    printf("Parent Hostname: %s\n", hostname_buf);
    printf("----------------------------------------\n");

    /**
     * 1. Create a new child process that runs child1_function
     * 2. The child process will have its own UTS and PID namespace
     * 3. You should pass the pointer to the pipefd array as an argument to the child1_function
     * 4. PID of child1 should be assigned to child_pid variable
     */

    // ------------------ WRITE CODE HERE ------------------

    child_pid = clone(child_function, child_stack + CHILD_STACK_SIZE, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, pipefd);

    if (child_pid == -1)
    {
        perror("clone");
        exit(EXIT_FAILURE);
    }

    // -----------------------------------------------------

    close(pipefd[1]);
    read(pipefd[0], buf, 1);
    close(pipefd[0]);

    /**
     * You can write any code here as per your requirement
     * Note: PID namespace of a process will only change the PID namespace of its subsequent children, not the process itself.
     * You are allowed to make modifications to the parent process such that PID namespace of child2 is same as that of child1
     */

    // ------------------ WRITE CODE HERE ------------------
     
    char pid_ns_path[128], uts_ns_path[128];
    snprintf(pid_ns_path, sizeof(pid_ns_path), "/proc/%d/ns/pid", child_pid);
    int pid_ns_fd = open(pid_ns_path, O_RDONLY);
    if (pid_ns_fd == -1)
    {
        perror("open pid ns");
        exit(1);
    }
    if (setns(pid_ns_fd, 0) == -1)
    {
        perror("setns pid");
        exit(EXIT_FAILURE);
    }
    // -----------------------------------------------------

    printf("----------------------------------------\n");
    printf("Parent Process PID: %d\n", getpid());
    gethostname(hostname_buf, 32);
    printf("Parent Hostname: %s\n", hostname_buf);
    printf("----------------------------------------\n");

    


    if (fork() == 0)
    {

        /**
         * 1. Join the existing UTS namespace and PID namespace
         */

        // ------------------ WRITE CODE HERE ------------------
        snprintf(uts_ns_path, sizeof(uts_ns_path), "/proc/%d/ns/uts", child_pid);
        int uts_ns_fd = open(uts_ns_path, O_RDONLY);
        if (uts_ns_fd == -1)
        {
            perror("open uts ns");
            exit(1);
        }
        if (setns(uts_ns_fd, 0) == -1)
        {
            perror("setns uts");
            exit(1);
        }
        


        
        close(uts_ns_fd);
        close(pid_ns_fd);

        // -----------------------------------------------------

        child2_function();

        
        exit(0);
    }

    wait(NULL);
    kill(child_pid, SIGKILL);
    wait(NULL);


    printf("----------------------------------------\n");
    printf("Parent Process PID: %d\n", getpid());
    gethostname(hostname_buf, 32);
    printf("Parent Hostname: %s\n", hostname_buf);
    printf("----------------------------------------\n");

    free(child_stack);
    return 0;
}
