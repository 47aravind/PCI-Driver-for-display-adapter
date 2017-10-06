# PCI-Driver-for-display-adapter

It is a PCI based linux driver for Display adapter. The user space applications can call thi driver functions for reading and writing into the device memory. While calling mmap() make sure to use the address 0xA0000000. 
