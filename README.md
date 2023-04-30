**mdadm linear device library**

The mdadm linear device is a storage utility tool that allows users to interact with and configure up to 1MB of data of JBOD devices at a time.

For example you could simultaneously manage 16 individual disks of 63,536 bytes each, as one disk. 

Some of its key features include reading, seeking and writing to any block of data on any disk.

This report will go into detail about the purpose and restrictions of every usable function in the library.

The mdadm linear device additionally supports caching to improve its efficiency and working with remote JBOD servers.

**Base Functions**
int mdadm_mount(): 
This function is used to mount the desired JBOD device to the mdadm linear device. This allows mdadm to read the number of disks as 1 whole continuous disk. This function takes no parameters. This function must be successfully used for any other base functions to work. Returns a 1 on success and -1 on failure.

int mdadm_unmount(): 
This function is used to unmount the JBOD device from the mdadm linear device. This function takes no parameters and should only be used after an instance of mdadm_mount was successful. The linear device can no longer interact with the stored data after this is used. Returns a 1 on success and -1 on failure.

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf): 
This function allows the user to read len bytes into the JBOD device starting at the provided address (addr), it stores the read data in the provided pointer to a buffer (*buf). In order to function properly, there must be a mounted JBOD device, the provided len must not exceed 1024 bytes, the addr must be within bounds of the JBOD device, and the pointer must not be pointing to NULL. Returns a 1 on success and -1 on failure.

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf): 
This function allows the user to write len bytes into the JBOD device starting at addr, *buf points to the buffer that contains the information the user wants to write to the device. Similarly to mdadm_read(), for this function to work properly, there must be a mounted JBOD device, the provided len must not exceed 1024 bytes, the addr must be within bounds of the device, and the pointer must not be pointing to NULL. Returns a 1 on success and -1 on failure.

**Cache Functions**
int cache_create(int num_entries):
This function allows the user to create a cache structure of the desired number of entries (num_entries). There must not already be a cache created when this function is used. There may be no less than 2 entries and no more than 4096 entries. When this function is used, the mdadm_linear device will be able to perform its actions more efficiently. Returns a 1 on success and -1 on failure.

int cache_destroy():
This function destroys the previously created cache by freeing all of its allocated data. There must be a created cache for this function to succeed and it takes no parameters. Ensure that you call this function when you are finished using the cache to prevent memory leaks. Returns a 1 on success and -1 on failure.

**Network Functions**
bool jbod_connect(const char *ip, uint16_t port):
This function allows the user to connect to a remote JBOD server given the correct *ip and port. Whereas the base functions would previously locally execute commands, they will now be sent over to the remote network for execution. In order to connect to the JBOD_SERVER, you should run this command with ip set to "127.0.0.1" and the port set to 3333. Returns true on success and false on failure.

void jbod_disconnect():
This function closes the connection between the client and the network. Ensure that you call this function when you are finished using the network to prevent several issues such as a waste of resources and an unstable, congested network. This function takes no parameters and returns nothing.
