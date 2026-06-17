/*
Tour.cpp -- Continuum scripted menu tour: drive the menu from a text script
            so menu GIFs can be captured reproducibly (tools/capture-menu-gif.sh).
Copyright (C) 2026 James Bishop

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
//
// A tour script is one verb per line; blank lines and lines starting with '#'
// are ignored. Verbs:
//
//   wait <ms>        pause before the next step (this is the ONLY thing that
//                    spends time; every other verb runs on the next frame)
//   click "<label>"  focus the visible item whose on-screen text matches
//                    <label> and activate it (real menu sound + action)
//   focus "<label>"  focus that item WITHOUT activating it, so a following
//                    `key left/right` can drive a slider/spinner for the demo
//   key <name>       send a navigation key to the current screen; <name> is one
//                    of up/down/left/right/enter/escape(=back)/tab/pgup/pgdn
//   back             same as `key escape` (close a sub-page / step back)
//   mark <label>     print "[ui_tour] MARK <label>" to the console/stdout; the
//                    capture wrapper watches mark rec_start / rec_stop to bracket
//                    the recording
//   inhibit_settings save + force the "clean capture" settings (engine version
//                    watermark scr_drawversion + console-notify con_notifytime
//                    off). con_notifytime=0 also hides the tour's own messages.
//   restore_settings put those settings back. The SCRIPT controls timing: call
//                    inhibit_settings before mark rec_start, and restore_settings
//                    AFTER mark rec_stop (with a small wait) so the restored
//                    overlay never lands in the GIF. Both are optional — omit
//                    them to record with the messages/watermark visible.
//
// Matching is case-insensitive and ignores surrounding whitespace, so the
// script reads like the UI: click "New Game". A missing label is reported with
// the list of items actually on screen, then the tour continues.
//
// Start a tour with:  ui_tour <scriptfile>   (path is in the game search path)
// Abort with:         ui_tour_stop
//
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "Continuum.h"
#include "keydefs.h"

enum
{
	STEP_WAIT = 0,
	STEP_CLICK,
	STEP_FOCUS,
	STEP_KEY,
	STEP_BACK,
	STEP_MARK,
	STEP_INHIBIT,   // inhibit_settings: save + force the managed settings to "clean"
	STEP_RESTORE,   // restore_settings: put the saved values back
};

#define TOUR_MAX_STEPS 256
#define TOUR_TEXT_LEN  64

struct tourStep_t
{
	int  type;
	int  arg;                  // ms for wait, key code for key
	char text[TOUR_TEXT_LEN];  // label for click/focus/mark
};

static struct
{
	bool       active;
	int        count;
	int        index;
	int        nextTime;       // ms (uiStatic.realTime scale); run current step once we reach it
	tourStep_t steps[TOUR_MAX_STEPS];

	// `inhibit_settings` saves the user's values for the managed settings and
	// forces them to a "clean" value; `restore_settings` puts them back. The tour
	// script drives the timing (so restore happens AFTER recording has stopped),
	// and may skip inhibit entirely to record with the overlay visible.
	bool       settingsSaved;
	float      savedVersion;   // scr_drawversion (engine version watermark)
	float      savedNotify;    // con_notifytime  (on-screen console message lifetime)
} s_tour;

// Save the managed settings and force them to their clean-capture value. Mirrors
// the map-capture clean-frame logic in hlsdk dlls/client.cpp. Currently the
// debugging overlay (version watermark + console notify); add more here as needed.
// Setting con_notifytime to 0 also clears the tour's own [ui_tour] notify lines.
static void Tour_InhibitSettings( void )
{
	if( s_tour.settingsSaved )
		return; // already inhibited; keep the originally-saved values
	s_tour.savedVersion = EngFuncs::GetCvarFloat( "scr_drawversion" );
	s_tour.savedNotify  = EngFuncs::GetCvarFloat( "con_notifytime" );
	s_tour.settingsSaved = true;
	EngFuncs::CvarSetValue( "scr_drawversion", 0.0f );
	EngFuncs::CvarSetValue( "con_notifytime", 0.0f );
}

// Restore the managed settings (safe to call when nothing was saved).
static void Tour_RestoreSettings( void )
{
	if( !s_tour.settingsSaved )
		return;
	EngFuncs::CvarSetValue( "scr_drawversion", s_tour.savedVersion );
	EngFuncs::CvarSetValue( "con_notifytime", s_tour.savedNotify );
	s_tour.settingsSaved = false;
}

// map a key name to its engine key code (menu navigation only)
static int Tour_KeyCode( const char *name )
{
	if( !stricmp( name, "up" ))     return K_UPARROW;
	if( !stricmp( name, "down" ))   return K_DOWNARROW;
	if( !stricmp( name, "left" ))   return K_LEFTARROW;
	if( !stricmp( name, "right" ))  return K_RIGHTARROW;
	if( !stricmp( name, "enter" ))  return K_ENTER;
	if( !stricmp( name, "escape" )) return K_ESCAPE;
	if( !stricmp( name, "back" ))   return K_ESCAPE;
	if( !stricmp( name, "tab" ))    return K_TAB;
	if( !stricmp( name, "pgup" ))   return K_PGUP;
	if( !stricmp( name, "pgdn" ))   return K_PGDN;
	return 0;
}

// read a label argument: a "quoted string", or the rest of the line trimmed
static void Tour_ReadLabel( const char *s, char *out, int outsz )
{
	int n = 0;
	while( *s && isspace( (unsigned char)*s )) s++;
	if( *s == '"' )
	{
		s++;
		while( *s && *s != '"' && n < outsz - 1 ) out[n++] = *s++;
	}
	else
	{
		while( *s && n < outsz - 1 ) out[n++] = *s++;
		while( n > 0 && isspace( (unsigned char)out[n - 1] )) n--; // trim trailing
	}
	out[n] = 0;
}

// case-insensitive compare that ignores surrounding whitespace on both sides
static bool Tour_LabelMatch( const char *item, const char *want )
{
	while( *item && isspace( (unsigned char)*item )) item++;
	while( *want && isspace( (unsigned char)*want )) want++;

	const char *ie = item + strlen( item );
	const char *we = want + strlen( want );
	while( ie > item && isspace( (unsigned char)ie[-1] )) ie--;
	while( we > want && isspace( (unsigned char)we[-1] )) we--;

	if(( ie - item ) != ( we - want ))
		return false;
	for( ; item < ie; item++, want++ )
		if( tolower( (unsigned char)*item ) != tolower( (unsigned char)*want ))
			return false;
	return true;
}

// find a visible, enabled item by its on-screen text; focus it and optionally fire it
static void Tour_Activate( const char *label, bool fire )
{
	CMenuBaseWindow *win = uiStatic.menu.Current();
	if( !win )
	{
		Con_Printf( "[ui_tour] no active menu for \"%s\"\n", label );
		return;
	}

	for( int i = 0; i < win->ItemCount(); i++ )
	{
		CMenuBaseItem *it = win->GetItemByIndex( i );
		if( !it || !it->szName || !it->IsVisible( ))
			continue;
		if( FBitSet( it->iFlags, QMF_GRAYED | QMF_INACTIVE ))
			continue;
		if( !Tour_LabelMatch( it->szName, label ))
			continue;

		win->SetCursorToItem( *it ); // focus (plays the move sound, like real nav)
		if( fire )
		{
			it->KeyDown( K_ENTER );  // press + release: real activation path
			it->KeyUp( K_ENTER );    // (CContButton fires + plays launch sound on up)
		}
		Con_Printf( "[ui_tour] %s \"%s\"\n", fire ? "clicking" : "focusing", label );
		return;
	}

	Con_Printf( "[ui_tour] NOT FOUND \"%s\"; visible items on this screen:\n", label );
	for( int i = 0; i < win->ItemCount(); i++ )
	{
		CMenuBaseItem *it = win->GetItemByIndex( i );
		if( it && it->szName && it->IsVisible( ))
			Con_Printf( "   \"%s\"\n", it->szName );
	}
}

// deliver a navigation key to the current screen (same path a real key takes)
static void Tour_Key( int code )
{
	CMenuBaseWindow *win = uiStatic.menu.Current();
	if( !win || !code )
		return;
	win->KeyDown( code );
	win->KeyUp( code );
}

static void Tour_Load( const char *path )
{
	int len = 0;
	char *file = (char *)EngFuncs::COM_LoadFile( path, &len );
	if( !file )
	{
		Con_Printf( "[ui_tour] cannot open \"%s\"\n", path );
		return;
	}

	Tour_RestoreSettings(); // a prior tour may have left settings inhibited
	s_tour.count = 0;

	char *p = file;
	while( *p && s_tour.count < TOUR_MAX_STEPS )
	{
		// pull one line
		char line[256];
		int n = 0;
		while( *p && *p != '\n' && *p != '\r' && n < (int)sizeof( line ) - 1 )
			line[n++] = *p++;
		line[n] = 0;
		while( *p == '\n' || *p == '\r' ) p++;

		// drop an inline comment: first '#' that isn't inside a "quoted label"
		bool inq = false;
		for( char *c = line; *c; c++ )
		{
			if( *c == '"' ) inq = !inq;
			else if( *c == '#' && !inq ) { *c = 0; break; }
		}

		// trim leading whitespace; skip blanks and comments
		char *s = line;
		while( *s && isspace( (unsigned char)*s )) s++;
		if( *s == 0 || *s == '#' )
			continue;

		// read the verb (longest is "inhibit_settings"/"restore_settings" = 16 chars)
		char verb[24];
		int v = 0;
		while( *s && !isspace( (unsigned char)*s ) && v < (int)sizeof( verb ) - 1 )
			verb[v++] = *s++;
		verb[v] = 0;
		while( *s && isspace( (unsigned char)*s )) s++;

		tourStep_t *st = &s_tour.steps[s_tour.count];
		memset( st, 0, sizeof( *st ));

		if( !stricmp( verb, "wait" ))
		{
			st->type = STEP_WAIT;
			st->arg = atoi( s );
		}
		else if( !stricmp( verb, "click" ))
		{
			st->type = STEP_CLICK;
			Tour_ReadLabel( s, st->text, sizeof( st->text ));
		}
		else if( !stricmp( verb, "focus" ))
		{
			st->type = STEP_FOCUS;
			Tour_ReadLabel( s, st->text, sizeof( st->text ));
		}
		else if( !stricmp( verb, "mark" ))
		{
			st->type = STEP_MARK;
			Tour_ReadLabel( s, st->text, sizeof( st->text ));
		}
		else if( !stricmp( verb, "key" ))
		{
			st->type = STEP_KEY;
			Tour_ReadLabel( s, st->text, sizeof( st->text )); // remember the name for the trace
			st->arg = Tour_KeyCode( st->text );
			if( !st->arg )
			{
				Con_Printf( "[ui_tour] unknown key \"%s\" (line skipped)\n", st->text );
				continue;
			}
		}
		else if( !stricmp( verb, "back" ))
		{
			st->type = STEP_BACK;
		}
		else if( !stricmp( verb, "inhibit_settings" ))
		{
			st->type = STEP_INHIBIT;
		}
		else if( !stricmp( verb, "restore_settings" ))
		{
			st->type = STEP_RESTORE;
		}
		else
		{
			Con_Printf( "[ui_tour] unknown verb \"%s\" (line skipped)\n", verb );
			continue;
		}

		s_tour.count++;
	}

	EngFuncs::COM_FreeFile( file );

	s_tour.index = 0;
	s_tour.nextTime = uiStatic.realTime;
	s_tour.active = ( s_tour.count > 0 );
	Con_Printf( "[ui_tour] loaded %d steps from \"%s\"\n", s_tour.count, path );
}

// called once per frame from UI_UpdateMenu after the clock is advanced
void UI_Tour_Think( void )
{
	if( !s_tour.active )
		return;
	if( uiStatic.realTime < s_tour.nextTime )
		return;

	if( s_tour.index >= s_tour.count )
	{
		Tour_RestoreSettings(); // never leave settings inhibited if restore was missing
		Con_Printf( "[ui_tour] DONE\n" );
		s_tour.active = false;
		return;
	}

	tourStep_t *st = &s_tour.steps[s_tour.index++];

	// non-wait steps run one-per-frame: leaving nextTime at the current time
	// means the guard above lets the next step fire on the following frame
	s_tour.nextTime = uiStatic.realTime;

	switch( st->type )
	{
	case STEP_WAIT:  Con_Printf( "[ui_tour] waiting %dms\n", st->arg ); s_tour.nextTime = uiStatic.realTime + st->arg; break;
	case STEP_CLICK: Tour_Activate( st->text, true ); break;   // prints "clicking ..."
	case STEP_FOCUS: Tour_Activate( st->text, false ); break;  // prints "focusing ..."
	case STEP_KEY:   Con_Printf( "[ui_tour] key %s\n", st->text ); Tour_Key( st->arg ); break;
	case STEP_BACK:    Con_Printf( "[ui_tour] back\n" ); Tour_Key( K_ESCAPE ); break;
	case STEP_MARK:    Con_Printf( "[ui_tour] MARK %s\n", st->text ); break;
	// inhibit BEFORE printing so con_notifytime=0 keeps this very line off screen
	case STEP_INHIBIT: Tour_InhibitSettings(); Con_Printf( "[ui_tour] inhibit settings\n" ); break;
	case STEP_RESTORE: Con_Printf( "[ui_tour] restore settings\n" ); Tour_RestoreSettings(); break;
	}
}

static void UI_Tour_f( void )
{
	if( EngFuncs::CmdArgc() < 2 )
	{
		Con_Printf( "usage: ui_tour <scriptfile>\n" );
		return;
	}
	Tour_Load( EngFuncs::CmdArgv( 1 ));
}

static void UI_TourStop_f( void )
{
	Tour_RestoreSettings(); // don't leave settings inhibited
	s_tour.active = false;
	Con_Printf( "[ui_tour] stopped\n" );
}

void UI_Tour_Init( void )
{
	EngFuncs::Cmd_AddCommand( "ui_tour", UI_Tour_f );
	EngFuncs::Cmd_AddCommand( "ui_tour_stop", UI_TourStop_f );
}
