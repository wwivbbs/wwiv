OBJ		= obj
BIN		= bin

LIBRARY         = $(BIN)/libtextui.a
DEMO_APP        = $(BIN)/textuidemo

CC              = g++

CFLAGS          = -I$(CURDIR)/include

##############################################################################
#
# Implicit Rules
#
#
#
# This whole substitution bit is because gcc doesn't like files
# with a 'CPP' extension, but it does like 'cpp'
#
#
#.SUFFIXES:
#.SUFFIXES: .o .c .cpp .C .CPP
#

$(OBJ)/%.o : src/%.cpp
	@echo Compiling $<
	@$(CC) -c $< $(CFLAGS) -o $@

#############################################################################
#
# Target "ALL" Rebuilds WWIV
#

LIB_OBJS =	       		\
	$(OBJ)/Colors.o		\
	$(OBJ)/Command.o	\
	$(OBJ)/Component.o	\
	$(OBJ)/Desktop.o	\
	$(OBJ)/InputBox.o	\
	$(OBJ)/Menu.o		\
	$(OBJ)/MenuBar.o	\
	$(OBJ)/MenuItem.o	\
	$(OBJ)/MessageBox.o	\
	$(OBJ)/PopupMenu.o	\
	$(OBJ)/SubWindow.o	\
	$(OBJ)/View.o		\
	$(OBJ)/Window.o		\

DEMO_OBJS =	       		\
	$(OBJ)/TUI.o		\

all: $(OBJ) $(BIN) $(LIBRARY) $(DEMO_APP)

clean:
	-rm -fr $(OBJ) $(BIN)

$(OBJ) $(BIN):
	-mkdir $@

$(LIBRARY): $(LIB_OBJS)
	@echo Linking $(LIBRARY)
	ar cru $(LIBRARY) $(LIB_OBJS)

$(DEMO_APP): $(DEMO_OBJS) $(LIBRARY)
	@echo Linking $(DEMO_APP)
	$(CC) -o $(DEMO_APP) $(DEMO_OBJS) $(LIBRARY) -lcurses
