# PostScript interpreter make file (Amiga) - Post V1.6
# This version assumes Lattice make (lmk), C (lc), asm, blink

# Debug symbols

#LCDBG  = -d0 -O           # no debug,            optimise
 LCDBG  = -d2 -O           # full debugging info, optimise
#LCDBG  = -d3              # full debugging info, flush regs

# Include debugging code

 LCDBC  =                  # no debugging code
#LCDBC  = -dFONTDEBUG      # font debugging code

# Cpu type and floating point (do not mix and match!)

 LCCPU  = -m0              # 68000 (or 68010/020/030)
 LCFPT  = -fl              # Lattice IEEE library
 LIBFPT = lib:lcm.lib      # Lattice IEEE library

#LCCPU  = -m2              # 68020 (or 68030)
#LCFPT  = -f8 -dM68881     # 68881 (or 68882)
#LIBFPT = lib:lcm881.lib   # 68881 (or 68882)

#

LIBOBJ = postinit.o postdata.o postchar.o postfont.o postgraph.o postshade.o postdraw.o postint.o postop1.o postop2.o postop3.o postop4.o

# Default target

all:            post postband postlj post.library

# Main program

post:           post.o postasm.o
	blink from lib:c.o post.o postasm.o to post lib lib:lc.lib smalldata nodebug

# Band printing driver

postband:       postband.o postasm.o
	blink from lib:c.o postband.o postasm.o to postband lib lib:lc.lib smalldata nodebug

# LaserJet driver

postlj:       postlj.o postasm.o
	blink from lib:c.o postlj.o postasm.o to postlj lib lib:lc.lib smalldata nodebug

# Shared library

post.library:	postlib.o $(LIBOBJ)
	blink from postlib.o $(LIBOBJ) to post.library lib $(LIBFPT) lib:lc.lib smalldata nodebug
	protect post.library rwd

# Statically linked version for debugging

poststat:       poststat.o postasm.o $(LIBOBJ)
	blink from lib:c.o poststat.o postasm.o $(LIBOBJ) to poststat lib $(LIBFPT) lib:lc.lib smalldata addsym
  
# The assembler routines contain FPU instructions

postasm.o:      postasm.a
        asm -u -m2 -iinclude: postasm.a

# The library structure

postlib.o:      postlib.a
        asm -u -iinclude: postlib.a

# The main program is compiled without stack checking as it contains a
# subtask, standard 68000 version only.

post.o:         post.c      postlib.h
        lc -v $(LCDBG) post.c

poststat.o:     post.c      postlib.h
        lc -v $(LCDBG) -dSTATICLINK -opoststat.o post.c

# The band printing driver, standard 6800 version only

postband.o:     postband.c  postlib.h
        lc $(LCDBG) postband.c


# The LaserJet driver, standard 6800 version only

postlj.o:       postlj.c    postlib.h
        lc $(LCDBG) postlj.c


# The library is compiled without stack checking, as it uses its caller's
# stack; 68000 and 68020/68881 versions.

.c.o:
        lc -v $(LCDBG) $(LCDBC) $(LCCPU) $(LCFPT) $*.c

postinit.o:     postinit.c  postlib.h post.h
postdata.o:     postdata.c  post.h
postchar.o:     postchar.c  post.h
postfont.o:     postfont.c  post.h
postgraph.o:    postgraph.c post.h
postshade.o:    postshade.c post.h
postdraw.o:     postdraw.c  post.h
postint.o:      postint.c   post.h
postop1.o:      postop1.c   post.h
postop2.o:      postop2.c   post.h
postop3.o:      postop3.c   post.h
postop4.o:      postop4.c   post.h
