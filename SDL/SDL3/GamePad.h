#ifndef winx68k_joy_h
#define winx68k_joy_h

#include "common.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_keycode.h"
#include "SDL3/SDL_gamepad.h"

#define	JOY_UP		0x01
#define	JOY_DOWN	0x02
#define	JOY_LEFT	0x04
#define	JOY_RIGHT	0x08
#define	JOY_TRGA	0x20
#define	JOY_TRGB	0x40
#define	JOY_HOME	0x80

#define	JOY_ThUP	0x01
#define	JOY_ThDN	0x02
#define	JOY_TRGC	0x04
#define	JOY_TRGD	0x08
#define	JOY_TRGE1	0x20
#define	JOY_TRGE2	0x40

#define JOYAXISPLAY 2048

#if defined(ANDROID) || TARGET_OS_IPHONE
#define VBTN_ON 2
#define VBTN_OFF 1

typedef struct _vbtn_points {
	float x;
	float y;
} VBTN_POINTS;
#endif

#define need_Vpad() (is_menu || Keyboard_IsSwKeyboard() || (!Config.JoyOrMouse && !sdl_joy))

uint8_t FASTCALL GamePad_Read(uint8_t num);
void FASTCALL GamePad_Write(uint8_t num, uint8_t data);

void GamePad_Init(void);
void GamePad_Open(void);
void GamePad_Change(uint32_t Pad_No);
void GamePad_Cleanup(void);
void GamePad_Add(SDL_JoystickID padid);
void GamePad_Removed(SDL_JoystickID padid);
void GamePad_Remapped(SDL_JoystickID padid);

void FASTCALL GamePadAxis_Update(int32_t which, uint8_t axis, int32_t value);
void FASTCALL GamePadButton_Update(int32_t which, uint8_t button, uint8_t on );
void Menu_GamePad_Update( SDL_Keycode key );

uint8_t get_joy_downstate(void);
void reset_joy_downstate(void);
uint8_t get_joy_upstate(void);
void reset_joy_upstate(void);

#if defined(ANDROID) || TARGET_OS_IPHONE
VBTN_POINTS *Joystick_get_btn_points(float scale);
void Joystick_Vbtn_Update(float scale);
uint8_t Joystick_get_vbtn_state(uint16_t n);
#endif

extern uint8_t JoyKeyState;

#endif
