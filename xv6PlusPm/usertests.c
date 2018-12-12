#define PGSIZE 4096
#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"


//#include "mmu.h"

struct trapframe {
  // registers as pushed by pusha
  uint edi;
  uint esi;
  uint ebp;
  uint oesp;      // useless & ignored
  uint ebx;
  uint edx;
  uint ecx;
  uint eax;

  // rest of trap frame
  ushort gs;
  ushort padding1;
  ushort fs;
  ushort padding2;
  ushort es;
  ushort padding3;
  ushort ds;
  ushort padding4;
  uint trapno;

  // below here defined by x86 hardware
  uint err;
  uint eip;
  ushort cs;
  ushort padding5;
  uint eflags;

  // below here only when crossing rings, such as from user to kernel
  uint esp;
  ushort ss;
  ushort padding6;
};

//PAGEBREAK!
//#ifndef __ASSEMBLER__
// Segment Descriptor
struct segdesc {
  uint lim_15_0 : 16;  // Low bits of segment limit
  uint base_15_0 : 16; // Low bits of segment base address
  uint base_23_16 : 8; // Middle bits of segment base address
  uint type : 4;       // Segment type (see STS_ constants)
  uint s : 1;          // 0 = system, 1 = application
  uint dpl : 2;        // Descriptor Privilege Level
  uint p : 1;          // Present
  uint lim_19_16 : 4;  // High bits of segment limit
  uint avl : 1;        // Unused (available for software use)
  uint rsv1 : 1;       // Reserved
  uint db : 1;         // 0 = 16-bit segment, 1 = 32-bit segment
  uint g : 1;          // Granularity: limit scaled by 4K when set
  uint base_31_24 : 8; // High bits of segment base address
};

typedef uint pte_t;

// Task state segment format
struct taskstate {
  uint link;         // Old ts selector
  uint esp0;         // Stack pointers and segment selectors
  ushort ss0;        //   after an increase in privilege level
  ushort padding1;
  uint *esp1;
  ushort ss1;
  ushort padding2;
  uint *esp2;
  ushort ss2;
  ushort padding3;
  void *cr3;         // Page directory base
  uint *eip;         // Saved state from last task switch
  uint eflags;
  uint eax;          // More saved state (registers)
  uint ecx;
  uint edx;
  uint ebx;
  uint *esp;
  uint *ebp;
  uint esi;
  uint edi;
  ushort es;         // Even more saved state (segment selectors)
  ushort padding4;
  ushort cs;
  ushort padding5;
  ushort ss;
  ushort padding6;
  ushort ds;
  ushort padding7;
  ushort fs;
  ushort padding8;
  ushort gs;
  ushort padding9;
  ushort ldt;
  ushort padding10;
  ushort t;          // Trap on task switch
  ushort iomb;       // I/O map base address
};

// PAGEBREAK: 12
// Gate descriptors for interrupts and traps
struct gatedesc {
  uint off_15_0 : 16;   // low 16 bits of offset in segment
  uint cs : 16;         // code segment selector
  uint args : 5;        // # args, 0 for interrupt/trap gates
  uint rsv1 : 3;        // reserved(should be zero I guess)
  uint type : 4;        // type(STS_{TG,IG32,TG32})
  uint s : 1;           // must be 0 (system)
  uint dpl : 2;         // descriptor(meaning new) privilege level
  uint p : 1;           // Present
  uint off_31_16 : 16;  // high bits of offset in segment
};


#include "proc.h"


/*struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};
*/



void
save(void)
{
	//open the files to save the pages, pcb, context, trapframes and the flags of pages
	int fdpage = open("backuppage", O_CREATE | O_RDWR);
   	int fd = open("backup", O_CREATE | O_RDWR);
    	int fdcont = open("backupcontext", O_CREATE | O_RDWR);
    	int fdtf = open("backuptf",  O_CREATE | O_RDWR);
    	int fdflag = open("backupflag",  O_CREATE | O_RDWR);
    
	//get pcb contents
	struct proc* t = malloc(sizeof(struct proc));
	pcbp((int)t);
	
	//keep total size of the pages because after malloc it'll change
	int pgtablesize = t->sz;
	
	//malloc the pointers and then passed to the pgsave syscall
	void* pgtable = malloc(pgtablesize);
	struct context* cptr = malloc(sizeof(struct context));
	struct trapframe* tfptr = malloc(sizeof(struct trapframe));
	void* flagptr = malloc((t->sz/PGSIZE)*sizeof(uint));
	pgsave(pgtable, cptr, tfptr, flagptr);
	
	//check if the second part is runnig with pc incremented in order not to continue from here
	if(tfptr->eax == 0) {
		return;
	}
	
	//check correct file open
    	if(fd >= 0 && fdpage >= 0 && fdcont >= 0 && fdtf >= 0 && fdflag >= 0) {
    	    //printf(1, "ok: create backup file succeed\n");
    	} else {
    	    printf(1, "error: create backup file failed\n");
    	    exit();
    	}
    
    	//write t process to file
    	int size = sizeof(*t);
   	if(write(fd, t, size) != size){
            printf(1, "error: write to backup file failed\n");
            exit();
    	}
        
   	//write pages & context & trapframe & flags of pages in file
  	write(fdpage, pgtable, pgtablesize);
    	write(fdcont, cptr, sizeof(struct context));
    	write(fdtf, tfptr, sizeof(struct trapframe));
	write(fdflag, flagptr, (t->sz/PGSIZE)*sizeof(uint));
    
    
    	//close all files
    	close(fd);
    	close(fdpage);
    	close(fdcont);
    	close(fdtf);
    	close(fdflag);
	
	//exit to stop the first part 
	exit();
}



int
main(int argc, char *argv[])
{
	int i = 0;
	for(i = 0; i < 20; i++) {
	    printf(1, "%d\n", i);
	    if(i == 10) {
		save();
	    }
	}
	exit();
	return 0;
}
