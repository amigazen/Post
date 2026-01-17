# PostScript interpreter make file (Amiga) - Post V1.6
# This version assumes Lattice make (lmk), C (sc), asm, blink

# Debug symbols

 SCDBG  = NODEBUG OPTIMIZE           # no debug,            optimise
#SCDBG  = DEBUG=S OPTIMIZE           # full debugging info, optimise
#SCDBG  = DEBUG=SF              # full debugging info, flush regs

# Include debugging code

 SCDBC  =                  # no debugging code
#SCDBC  = DEFINE=FONTDEBUG      # font debugging code

# Cpu type and floating point (do not mix and match!)

#SCCPU  = CPU=ANY              # 68000 (or 68010/020/030)
#SCFPT  = MATH=STANDARD              # Lattice IEEE library
#LIBFPT = lib:scm.lib      # Lattice IEEE library

SCCPU  = CPU=68030              # 68020 (or 68030)
SCFPT  = MATH=68881 DEFINE=M68881     # 68881 (or 68882)
LIBFPT = lib:scm881.lib   # 68881 (or 68882)

#

LIBOBJ = postinit.o postdata.o postchar.o postfont.o postgraph.o postshade.o postdraw.o postint.o postop1.o postop2.o postop3.o postop4.o

# Default target

#all:            post postband postlj post.library
all:             post post.library

# Main program

post:           post.o postasm.o Post_interface.o
	slink from lib:c.o post.o postasm.o Post_interface.o to post lib lib:sc.lib lib:amiga.lib smalldata nodebug

# Band printing driver

postband:       postband.o postasm.o
	slink from lib:c.o postband.o postasm.o to postband lib lib:sc.lib smalldata nodebug

# LaserJet driver

postlj:       postlj.o postasm.o
	slink from lib:c.o postlj.o postasm.o to postlj lib lib:sc.lib smalldata nodebug

# Shared library

post.library:	postlib.o $(LIBOBJ)
#	slink libfd post.fd libid "PostLib 1.87enh (10 Feb 1993)" to post.library from lib:libent.o lib:libinitr.o postlib.o $(LIBOBJ) lib $(LIBFPT) lib:sc.lib lib:amiga.lib libVersion 18 libRevision 5 smalldata nodebug
        slink to post.library from postlib.o $(LIBOBJ) lib $(LIBFPT) lib:sc.lib smalldata nodebug
	protect post.library rwd

# Statically linked version for debugging

poststat:       poststat.o postasm.o $(LIBOBJ)
	slink from lib:c.o poststat.o postasm.o $(LIBOBJ) to poststat lib $(LIBFPT) lib:sc.lib smalldata addsym
  
# The assembler routines contain FPU instructions

postasm.o:      postasm.a
        asm -u -m2 -iinclude: postasm.a

# The user interface -- R. Poole 2/3/92

Post_interface.o:       Post_interface.c  Post_interface.h
        sc NOSTKCHK $(SCDBG) Post_interface.c

# The library structure

postlib.o:      postlib.a
        asm -u -iinclude: postlib.a

# The main program is compiled without stack checking as it contains a
# subtask, standard 68000 version only.

post.o:         post.c      postlib.h   Post_interface.h
        sc NOSTKCHK $(SCDBG) post.c

poststat.o:     post.c      postlib.h
        sc NOSTKCHK $(SCDBG) -dSTATICLINK -opoststat.o post.c

# The band printing driver, standard 6800 version only

postband.o:     postband.c  postlib.h
        sc $(SCDBG) postband.c


# The LaserJet driver, standard 6800 version only

postlj.o:       postlj.c    postlib.h
        sc $(SCDBG) postlj.c


# The library is compiled without stack checking, as it uses its caller's
# stack; 68000 and 68020/68881 versions.

.c.o:
        sc NOSTKCHK $(SCDBG) $(SCDBC) $(SCCPU) $(SCFPT) $*.c

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
