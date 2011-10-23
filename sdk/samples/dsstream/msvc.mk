NAME = dsstream
EXT = exe

GOALS = $(NAME).$(EXT)

LIBS    =kernel32.lib user32.lib dsound.lib comdlg32.lib gdi32.lib \
         winmm.lib comctl32.lib libc.lib advapi32.lib

OBJS    =  dsstream.obj dstrtime.obj dstrwave.obj wassert.obj debug.obj dstrenum.obj
          
!if "$(DEBUG)" == "debug"
COPT =-YX -DDEBUG -Zi -Fd$(NAME).PDB
LOPT =-debug:full -debugtype:cv -pdb:$(NAME).pdb
ROPT =-DDEBUG
!else
COPT =-YX
LOPT =-debug:none
ROPT =
!endif
DEF = $(NAME).def
RES = $(NAME).res

CFLAGS  =$(COPT) -Oxa -D_X86_ $(CDEBUG) -Fo$@
LFLAGS  =$(LOPT)
RCFLAGS =$(ROPT)

NOLOGO = 1

!include ..\..\mssdk.mk

$(NAME).$(EXT): \
        $(OBJS) ..\$(NAME).def $(RES)
        @$(LINK) $(LFLAGS) @<<
-out:$(NAME).$(EXT)
-map:$(NAME).map
-machine:i386
-subsystem:windows,4.0
-def:..\$(NAME).def
$(LIBS)
$(RES)
$(OBJS)
<<
