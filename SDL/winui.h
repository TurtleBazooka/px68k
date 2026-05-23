#ifndef _winx68k_winui_h
#define _winx68k_winui_h

#include "common.h"

extern	int32_t	Debug_Text, Debug_Grp, Debug_Sp;
extern	int32_t	LastClock[4];

extern char cur_dir_str[];
extern int32_t cur_dir_slen;
extern int32_t menu_state;

void WinUI_Init(void);
int32_t WinUI_Menu(int32_t first);
float WinUI_get_vkscale(void);
void send_key(void);

#define WUM_MENU_END 1
#define WUM_EMU_QUIT 2

enum MenuState {ms_key, ms_value, ms_file, ms_hwjoy_set};

#define MFL_MAX 1000

struct menu_flist {
	char name[MFL_MAX][MAX_PATH];
	char type[MFL_MAX];
	char dir[4][MAX_PATH];
	int32_t ptr;
	int32_t num;
	int32_t y;
};

extern char menu_item_key[][19];
extern char menu_items[][19][100];

// GamePad nameを格納するmenu_itemは何番目？
#define GamePad_menu_item   14
// MIDI IN/OUTを格納するmenu_itemは何番目？
#define MIDI_OUT_menu_item   9
#define MIDI_IN_menu_item   10

extern int32_t JoyDirection;

int32_t WinUI_get_drv_num(int32_t key);

#ifndef _winx68k_gtkui_h
#define _winx68k_gtkui_h
#endif //winx68k_gtkui_h
#endif //winx68k_winui_h
