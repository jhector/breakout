breakout
========

In this challenge we need to use DWARF bytecode to call a libc function with our parameters. DWARF bytecode is responsible for stack unwinding when an exception was triggered, it operates on the stack and is touring complete.

If you want to know more about DWARF bytecode, check out the paper *Exploiting the hard-working DWARF: Trojan and Exploit Techniques With No Native Executable Code* by James Oakley and Sergey Bratus, you can find it [here](https://www.usenix.org/legacy/event/woot11/tech/final_files/Oakley. pdf).

## Summary

* create a fake program header, .eh_frame_hdr section, .eh_frame section and .gcc_except_table section in heap
* overwrite the cached program header pointer inside libgcc (p_eh_frame_hdr in struct frame_hdr_cache_element)
* trigger an exception to run our DWARF code inside our faked .eh_frame section
* the DWARF code does the following:
    * prepare a small ROP chain on the heap
    * ROP chain will pop **rdi**
    * final return is to  **system()**
    * pivot **rsp** to point to new **rsp** value
    * return to **pop rsp** to set **rsp** to our ROP chain

tl;dr at bottom of the page

## Write-up
The bug in the application is fairly simple, when free()ing a prisoner, the free()ed element is still present inside the linked list.
Using the note functionality we can allocate a block of the same size and control the entire struct.

```cpp
typedef struct prisoner_s {
    const char *risk;
    char *name;
    char *aka;
    uint32_t age;
    uint32_t cell;
    char *sentence;
    uint32_t note_size;
    char *note;
    struct prisoner_s *next;
} Prisoner;
```

**malloc()** is used, so the requested memory won't be initialized (unlike **calloc()**) so all previous values are still present. 
Since we can control the entire struct, we can build an arbitrary leak using the *note* pointer and the **list()** function (which also prints the note if present).

Each element has a pointer inside the memory region of the mapped binary, because of the following code:

```cpp
curr->risk = "High";
```

In order to leak the address we will create a note of size 10 for the prisoner element that we control. The allocated block is located before the prisoner elements. Now we modify the prisoner struct that we control. In particular we set *note_size* to a higher value than 10, so that it will print data beyond the allocated chunk (10 byte chunk).

```
+---------+<----+
| 10 Byte |     |
+---------+     |
...             |
+----------+    |
| prisoner |    |
|- ptr risk|    |
|...       |    |
+----------+    |
...             |
+------------+  |
| controlled |  |
| ...        |  |
| note_size  |  |
| note ptr   | -+
| ...        |
+------------+
```

when calling **list()** it will print *note_size* bytes, with a big enough size, we will get the pointer.

We could also use the **note()** function to create an arbitrary write, but the note pointer is limited to a whitelist. It gives us only access to the heap region and a part inside libgcc (we will get to this later).

In order to exploit it without libc offsets, we can read a GOT entry and use that libc pointer to retrieve the base of libc by scanning for the ELF header. With the libc base address, strtab and symtab, we can now resolve a symbol inside libc, like **system()**.

So now that we got the address of **system()** we need a way to call it. This is where the DWARF bytecode comes into play.
libgcc caches a pointer to the gnu_eh_frame program header inside a struct array:

```c
#define FRAME_HDR_CACHE_SIZE 0x7
...
static struct frame_hdr_cache_element
{
    _Unwind_Ptr pc_low;
    _Unwind_Ptr pc_high;
    _Unwind_Ptr load_base;
    const ElfW(Phdr) *p_eh_frame_hdr;
    const ElfW(Phdr) *p_dynamic;
    struct frame_hdr_cache_element *link;
} frame_hdr_cache[FRAME_HDR_CACHE_SIZE];
```

the pointer of interest is **p_eh_frame_hdr** (it is a little confusing, but p_eh_frame_hdr doesn't point to .eh_frame_hdr, instead it points to the gnu_eh_frame program header). We need to overwrite this pointer so that the next time an exception occurs our supplied bytecode will be used to unwind the stack.

We use our almost arbitrary write to create all fake sections inside the heap.

Since the binary is compiled with -fPIC, all offsets inside program header and .eh_frame_hdr are relative to the image base (we need to keep that in mind while crafting the sections.

We lay out the sections inside the heap so that, first we have our gnu_eh_frame program header, after that the .eh_frame_hdr, 0x60 bytes after the .eh_frame_hdr we put .eh_frame, and 0x200 bytes after .eh_frame we put .gcc_except_table.

```
heap+offset
+------------------+
| gnu_eh_frame     |
+------------------+
| eh_frame_hdr     |
+------------------+
+0x60
+------------------+
| eh_frame         |
+------------------+
+0x200
+------------------+
| gcc_except_table |
+------------------+
```

Using the tool **katana** we can extract the dwarf bytecode from the binary and create a template. The following parts needs to be edited inside the extracted code:

```
section_type: ".eh_frame"
section_addr: {section_addr}
eh_hdr_addr: {eh_hdr_addr}
except_table_addr: {except_table_addr}
...
DW_CFA_val_expression r7
begin EXPRESSION
DW_OP_breg7 {rsp_offset}
end EXPRESSION
DW_CFA_val_expression r6
begin EXPRESSION
DW_OP_constu {high}
DW_OP_constu 0x20
DW_OP_shl
DW_OP_constu {low}
DW_OP_plus
end EXPRESSION
...
landing_pad: 0x256
```

The first block will be filled with the relative addresses to the fake sections inside the heap. Second block will be inserted into the correct FDE entry of the function where the exception is triggered. **landing_pad** is the offset from the beginning of the function that contains the catch block. This is the offset where the exception should return to. We change it so that it jumps to a **pop rsp**.

**pop r12** is a 2 byte instruction, when jumping to the second byte of the instruction it becomes **pop rsp**. So it shouldn't be too hard to find that gadget.

```
DW_CFA_val_expression r7
begin EXPRESSION
...
end EXPRESSION
```

this will set r7 register to the result of the expression, r7 in DWARF is RSP when the exception returns. With **DW_OP_breg7** we read the value of RSP ans add or subtract an offset to the value. This expression will set the stack pointer to point to our new RSP value, remember that we jump to **pop rsp** instead of the actual catch block.

Using the following:
```
DW_CFA_val_expression r6
begin EXPRESSION
...
end EXPRESSION
```

we will place the new stack pointer value to the location we set RSP to. So that when **pop rsp** is executed, it will pop our value into RSP.

* **DW_OP_constu** places a value
* **DW_OP_constu** places another value
* **DW_OP_shl** shifts the first value to the lift by the amount of bits specified by the second value
* **DW_OP_constu** places another value
* **DW_OP_plus** adds the shifted value and the newly placed one

The result of **DW_OP_plus** will be stored in r6 (RBP) which is not really important, but now r7 points to that value, and **pop rsp** will now set RSP to that value. You may ask yourself why not just use **DW_OP_constu** to place the value there, well we need to place a 64 bit value, which can't be done using **DW_OP_constu** (somehow it is limited to 32 bit).

The value of r6 points to our ROP chain inside the heap. Our ROP chain is fairly simple.

Exception returns to:
pop rsp; pop r13; pop r14; pop r15;

Heap ROP (needs to contain r13 - r15)
```
+-------------+
| -1          |
+-------------+
| -1          |
+-------------+
| -1          |
+-------------+
| pop rdi     |
+-------------+
| /bin/bash   |
+-------------+
| system()    |
+-------------+

```

From there it is just simple ROP which executes **system()** with */bin/bash*.

tl;dr we use the DWARF bytecode to set RSP to the ROP chain in the heap which pops RDI and executes system()
