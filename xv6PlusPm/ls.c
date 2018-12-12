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

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;
  
  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  
  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  
  if((fd = open(path, 0)) < 0){
    printf(2, "ls: cannot open %s\n", path);
    return;
  }
  
  if(fstat(fd, &st) < 0){
    printf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }
  
  switch(st.type){
  case T_FILE:
    printf(1, "%s %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
    break;
  
  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf(1, "ls: cannot stat %s\n", buf);
        continue;
      }
      printf(1, "%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}


void
load(void)
{

   	//open files with the process details
    	int fd = open("backup", O_RDONLY);
    	int fdpage = open("backuppage", O_RDONLY);
    	int fdcont = open("backupcontext", O_RDONLY);
    	int fdtf = open("backuptf", O_RDONLY);
    	int fdflag = open("backupflag", O_RDONLY);
    
		
    	// check correct open
    	if(fd >= 0 && fdpage >= 0 && fdcont >= 0 && fdtf >= 0 && fdflag >= 0) { 
        	//printf(1, "ok: read backup file succeed\n");
    	} else {
        	printf(1, "error: read backup file failed\n");
        	exit();
    	}
    
   	//read pcb file contents	
    	struct proc *t = malloc(sizeof(struct proc));	
    	int size = sizeof(*t);
    	if(read(fd, t, size) != size){
        	printf(1, "error: read from backup file failed. %dB\n", sizeof(*t));
        	exit();
    	}
	
	//initial total size of pages from read pcb			
	int pgtablesize = t->sz;

	//read from pages file
	void* pgtable = malloc(pgtablesize);
	
	int readret = read(fdpage, pgtable, (t->sz));
	if(!readret) {
		printf(1, "error: read from backup file failed(page read), read return %d %d.\n", readret, pgtablesize);
	}
	
 	//read context from file
	struct context* cptr = malloc(sizeof(struct context));
	read(fdcont, cptr, sizeof(struct context));
	
	//read trapframe from file
	struct trapframe* tfptr = malloc(sizeof(struct trapframe));
	read(fdtf, tfptr, sizeof(struct trapframe));

	//read flags of pages from file
	void* flagptr = malloc((t->sz/PGSIZE)*sizeof(uint));	
	read(fdflag, flagptr, ((t->sz)/PGSIZE)*sizeof(uint));
	
   	//close all open files
   	close(fd);
    	close(fdpage);
    	close(fdcont);
    	close(fdtf);
    	close(fdflag);

	//call pcbload syscall to resume stopped process
    	pcbload(t, pgtable, cptr, tfptr, flagptr);
    	
}

int
main(int argc, char *argv[])
{
	load();
	wait();
	exit();
}
