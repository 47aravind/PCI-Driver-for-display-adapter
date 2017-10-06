#include <linux/module.h>	// for init_module() 
#include <linux/pci.h>		// for pci_find_class() 

#define VENDOR_ID 0x8086	// Silicon Integrated Systems
#define DEVICE_ID 0x2562	// SiS-315 Graphics Processor

char modname[] = "vramm";	// used for displaying driver-name
int my_major = 0;		// driver's assigned major-number
unsigned long fb_base;		// physical address of frame-buffer
unsigned long fb_size;		// size of the frame-buffer (bytes)

int loff_t my_lseek( struct file *file, loff_t pos, int whence )
{
	loff_t	newpos = -1;

	switch ( whence )
		{
		case 0:	newpos = pos; break;			// SEEK_SET
		case 1:	newpos = file->f_pos + pos; break;	// SEEK_CUR
		case 2: newpos = fb_size + pos; break;		// SEEK_END
		}
	
	if (( newpos < 0 )||( newpos > fb_size )) return -EINVAL;
	file->f_pos = newpos;
	return	newpos;
}	

int
 my_write( struct file *file, const char *buf, size_t count, loff_t *pos )
{
	unsigned long	phys_address = fb_base + *pos;
	unsigned long	page_number = phys_address / PAGE_SIZE;
	unsigned long	page_indent = phys_address % PAGE_SIZE;
	unsigned long	where;
	void		*vaddr;

	// sanity check: we cannot write anything past end-of-vram
	if ( *pos >= fb_size ) return 0;

	// we can only write up to the end of the current page
	if ( page_indent + count > PAGE_SIZE ) count = PAGE_SIZE - page_indent;

	// ok, map the current page of physical vram to virtual address
	where = page_number * PAGE_SIZE;	// page-addrerss
	vaddr = ioremap( where, PAGE_SIZE );	// setup mapping
	
	// copy 'count' bytes from the caller's buffer to video memory
	memcpy_toio( vaddr + page_indent, buf, count );
	iounmap( vaddr );			// unmap memory
	
	// tell the kernel how many bytes were actually transferred
	*pos += count;
	return	count;
}

ssize_t	
my_read( struct file *file, char *buf, size_t count, loff_t *pos )
{
	unsigned long	phys_address = fb_base + *pos;
	unsigned long	page_number = phys_address / PAGE_SIZE;
	unsigned long	page_indent = phys_address % PAGE_SIZE;
	unsigned long	where;
	void		*vaddr;

	// sanity check: we cannot read anything past end-of-vram
	if ( *pos >= fb_size ) return 0;

	// we can only read up to the end of the current page
	if ( page_indent + count > PAGE_SIZE ) count = PAGE_SIZE - page_indent;

	// ok, map the current page of physical vram to virtual address
	where = page_number * PAGE_SIZE;	// page-addrerss
	//ioremap is used to map physical memory into virtual address space of the kernel. 
	vaddr = ioremap( where, PAGE_SIZE );	// setup mapping
	
	// copy 'count' bytes from video memory to the caller's buffer 
	//vaddr - virtual address
	memcpy_fromio( buf, vaddr + page_indent, count );
	iounmap( vaddr );			// unmap memory
	
	// tell the kernel how many bytes were actually transferred
	*pos += count;
	return	count;
}
       /*
	void *mmap(void *addr, size_t length, int prot, int flags,int fd, off_t offset);
	mmap() creates a new mapping in the virtual address space of the
       calling process.  The starting address for the new mapping is
       specified in addr.  The length argument specifies the length of the
       mapping.*/


int my_mmap( struct file *file, struct vm_area_struct *vma )
{
	unsigned long	region_origin = vma->vm_pgoff * PAGE_SIZE;
	unsigned long	region_length = vma->vm_end - vma->vm_start;
	unsigned long	physical_addr = fb_base + region_origin;	
	unsigned long	user_virtaddr = vma->vm_start;
	
	// sanity check: mapped region cannot expend past end of vram
	if ( region_origin + region_length > fb_size ) 
		return -EINVAL;
	
	// let the kernel know not to try swapping out this region
	vma->vm_flags = VM_RESERVED;

	// invoke kernel function that sets up the page-table entries
	//mmap() allocates the virtual pages and allocates into the page 
	// directory of the calling process and  remap_pfn_range() is used to map
	//the virtual address into the physical address.This is done to access the memory from user space.
	//building new page tables to map a range of physical addresses 
	if ( remap_pfn_range( vma, user_virtaddr, physical_addr >> PAGE_SHIFT,
		region_length, vma->vm_page_prot ) )
		 return -EAGAIN;

	return	0;  // SUCCESS
}

struct file_operations my_fops = {
				owner:	THIS_MODULE,
				llseek:	my_lseek,
				write:	my_write,
				read:	my_read,
				mmap:	my_mmap,
				};

int init_module( void )
{
	static struct pci_dev *devp = NULL;

	printk( "<1>\nInstalling \'%s\' module ", modname );
	printk( "(major=%d) \n", my_major );
	
	/*Iterates through the list of known PCI devices. If a PCI device is found with a matching vendor and device, a pointer to its device structure is returned .A new search is initiated by passing NULL as the from argument. Otherwise if from is not NULL, searches continue from next device on the global list.The new function now is pci_get_device()*/
	// identify the video display device
	
	devp = pci_find_device( VENDOR_ID, DEVICE_ID, devp );
	if ( !devp ) return -ENODEV;
	/*The function returns the first address (memory address or I/O port number) associated with one of the six PCI I/O regions. The region is selected by the integer bar (the base address register), ranging from 0-5 (inclusive).*/
	
	// determine location and length of the frame buffer
	fb_base = pci_resource_start( devp, 0 );
	fb_size = pci_resource_len( devp, 0 );//Returns the byte length of a PCI region.
	printk( "<1>  address-range of frame-buffer: " );
	printk( "%08lX-%08lX ", fb_base, fb_base+fb_size );
	printk( "(%lu MB) \n", fb_size >> 20 );

	return	register_chrdev( my_major, modname, &my_fops );//Register a major number for character 								//devices.
}

void cleanup_module( void )
{
	unregister_chrdev( my_major, modname );
	printk( "<1>Removing \'%s\' module\n", modname );
}

MODULE_LICENSE("GPL");
