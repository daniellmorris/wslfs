## WSLFS - Windows Subsystem for Linux File System (For Linux)

The purpose of this repository is to create a file system for linux 
(Not the windows linux subsystem) that can read the windows subsystem file system. 
This would allow someone to log into there linux partition on a dual boot system 
and access there windows windows wsl filesystem. 

NOTE: This probably needs more testing. This seems to work with regular files/folders 
and with symlinks but has not been tested with any other file types and likely will not
work with any other file types.

If you have different user mapping from those in your Windows Subsystem for Linux then
I reccomend you use bindfs to map your username and possibly group names to your current
system to avoid messing up your windows wsl permissions.

## Usage

Use ntfs-3g to mount the windows partition (Make sure it is the latest version)

```
./wslfs -s <WindowsPartitionMountPointFromNtfs3g/Users/**windows_user**/AppData/Local/lxss/home/**wsl_user**> <mountpoint> [OPTIONS]
```

To umount use the fusermount command.
```
fusermount -zu <mountpoint>
```

## Dependencies

To run and build *wslfs* the following dependencies are required.

* libfuse-dev
* cmake
* ntfs-3g

For a debian based distribution you can install libfuse-dev using apt-get 
```
apt-get install libfuse-dev cmake
```

ntfs-3g may have to be installed from source.
Follow instructions at http://www.tuxera.com/community/open-source-ntfs-3g/

I am using version ntfs-3g version ntfs-3g 2016.2.22 (We will consider that the minimum version)

## Build from source

To build from source, do the following, make sure that you satisfy the
dependencies. Check-out the repository and run:

```
cd wslfs
mkdir build
cd build
cmake ../src
make
make install
```

If you want to install PTFS into a certain directory simply set the
CMAKE_INSTALL_PREFIX to the appropriate directory.
```
cmake ../ -DCMAKE_INSTALL_PREFIX=/tmp/foo
```
will install into /tmp/foo.

## LICENSE

WSLFS is distributed under the terms of the  MIT licence. See the LICENSE file for the full license text
