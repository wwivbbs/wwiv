#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#endif

#include <stdio.h>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <curses.h>
#undef box
#undef clear
#undef erase
#undef move
#undef refresh

#include "textui/Colors.h"
#include "textui/Component.h"
#include "textui/Command.h"
#include "textui/View.h"
#include "textui/Desktop.h"
#include "textui/Menu.h"
#include "textui/MenuBar.h"
#include "textui/MenuItem.h"
#include "textui/MessageBox.h"
#include "textui/PopupMenu.h"
#include "textui/Window.h"
#include "textui/SubWindow.h"
#include "textui/InputBox.h"
