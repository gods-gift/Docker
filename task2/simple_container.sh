#!/bin/bash

SIMPLE_CONTAINER_ROOT=container_root

mkdir -p $SIMPLE_CONTAINER_ROOT

gcc -o container_prog container_prog.c

## Subtask 1: Execute in a new root filesystem

cp container_prog $SIMPLE_CONTAINER_ROOT/

# 1.1: Copy any required libraries to execute container_prog to the new root container filesystem 
EXECUTABLE="container_prog"
DEP_FOLDER=$SIMPLE_CONTAINER_ROOT

# # Create the Dependencies folder if it doesn't exist
# if [ ! -d "$DEP_FOLDER" ]; then
#     mkdir "$DEP_FOLDER"
#     echo "Folder '$DEP_FOLDER' created successfully."
# else
#     echo "Folder '$DEP_FOLDER' already exists."
# fi

# Check if the executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: The executable '$EXECUTABLE' does not exist."
    exit 1
fi

# Use ldd to get the dependencies and copy them to the Dependencies folder
# Get the list of shared libraries using ldd
LIBS=$(ldd "$EXECUTABLE" | awk '{if ($3 ~ /^\//) print $3; else if ($1 ~ /^\//) print $1}')

# Copy each library to the container_root
for lib in $LIBS; do
    DEST="$SIMPLE_CONTAINER_ROOT$(dirname "$lib")"
    mkdir -p "$DEST"
    cp "$lib" "$DEST/"
done

echo -e "\n\e[1;32mOutput Subtask 2a\e[0m"

# 1.2: Execute container_prog in the new root filesystem using chroot. You should pass "subtask1" as an argument to container_prog
sudo chroot container_root ./container_prog subtask1


echo "__________________________________________"
echo -e "\n\e[1;32mOutput Subtask 2b\e[0m"
## Subtask 2: Execute in a new root filesystem with new PID and UTS namespace
# The pid of container_prog process should be 1
# You should pass "subtask2" as an argument to container_prog
# sudo unshare --pid --uts --mount-proc chroot container_root ./container_prog subtask2

sudo unshare --fork --uts --pid --mount-proc chroot container_root ./container_prog subtask2



echo -e "\nHostname in the host: $(hostname)"


# ## Subtask 3: Execute in a new root filesystem with new PID, UTS and IPC namespace + Resource Control
# # Create a new cgroup and set the max CPU utilization to 50% of the host CPU. (Consider only 1 CPU core)

sudo mkdir -p /sys/fs/cgroup/cpu_limit_group

# echo "+cpu +memory" | sudo tee /sys/fs/cgroup/cpu_limit_group/cgroup.subtree_control > /dev/null

echo "50000 100000" | sudo tee /sys/fs/cgroup/cpu_limit_group/cpu.max > /dev/null


echo "__________________________________________"
echo -e "\n\e[1;32mOutput Subtask 2c\e[0m"
# # Assign pid to the cgroup such that the container_prog runs in the cgroup
# # Run the container_prog in the new root filesystem with new PID, UTS and IPC namespace
# # You should pass "subtask1" as an argument to container_prog

echo $$ | sudo tee "/sys/fs/cgroup/cpu_limit_group/cgroup.procs" > /dev/null

sudo unshare --fork --uts --pid --uts --mount-proc chroot container_root ./container_prog subtask3

# # Remove the cgroup
sudo rmdir "/sys/fs/cgroup/cpu_limit_group" 2>/dev/null

# # If mounted dependent libraries, unmount them, else ignore
