NAME = dsshow
EXT = exe

GOALS = $(NAME).$(EXT)

LIBS    =kernel32.lib user32.lib advapi32.lib ddraw.lib dsound.lib \
         comdlg32.lib gdi32.lib winmm.lib msacm32.lib libc.lib uuid.lib ole32.lib comctl32.lib

OBJS    =  shell.obj wassert.obj wave.obj dsenum.obj
          
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