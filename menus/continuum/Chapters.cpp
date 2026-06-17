/*
Chapters.cpp -- Continuum chapter picker: chapter list + thumbnail preview
Copyright (C) 2026 a1batross, James Bishop

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#include <ctype.h>
#include "Continuum.h"
#include "YesNoMessageBox.h"
#include "keydefs.h"

using namespace Cont;

#define ROW_H          46
#define ROW_GAP        4
#define CONTENT_TOP    208
#define CONTENT_BOTTOM ( 768 - LEGEND_H - 22 )
#define PANEL_W        560   // thumbnail panel on the right (4:3)

// builds "gfx/shell/chapters_<gamefolder>.lst"; the file's presence is what
// gates the Chapters button (see CMenuContGamePage)
void Cont_ChaptersListPath( const char *folder, char *out, int size )
{
	snprintf( out, size, "gfx/shell/chapters_%s.lst", folder );
}

class CMenuContChapters : public CMenuFramework
{
public:
	CMenuContChapters() : CMenuFramework( "CMenuContChapters" ),
		m_iSel( 0 ), m_flScroll( 0 ), m_flScrollTarget( 0 ) { }

	bool KeyDown( int key ) override;
	bool KeyUp( int key ) override;
	bool MouseMove( int x, int y ) override;
	void Draw() override;
	void Show() override;

private:
	void _Init() override;
	void _VidInit() override;

	struct entry_t
	{
		char map[64];
		char name[96];
		char loadout[256]; // space-separated entity classnames, "" for none
	};

	void Refresh();
	void Select( int i, bool playSound = true );
	void Activate();      // confirm if a game is running, else launch
	void StartChapter();  // set cvars + newgame "<map>"
	void EnsureVisible();
	int RowAtCursor();
	int ListW() const { return uiStatic.width - 2 * MARGIN - PANEL_W - 24; }

	CUtlVector<entry_t> m_Chapters;
	CImage m_Shot;
	int m_iSel;
	float m_flScroll, m_flScrollTarget;
	bool m_bMousePressed = false;

	CMenuYesNoMessageBox dialog;
};

void CMenuContChapters::_Init()
{
	dialog.Link( this );
}

void CMenuContChapters::_VidInit()
{
	VidInitFonts();
}

void CMenuContChapters::Show()
{
	CMenuFramework::Show();
	m_iSel = 0;
	m_flScroll = m_flScrollTarget = 0;
	Refresh();
}

void CMenuContChapters::Refresh()
{
	m_Chapters.RemoveAll();

	char path[128];
	Cont_ChaptersListPath( gMenu.m_gameinfo.gamefolder, path, sizeof( path ));

	int len = 0;
	char *afile = (char *)EngFuncs::COM_LoadFile( path, &len );
	if( afile )
	{
		char *pfile = afile;
		char token[256];

		// each entry is three tokens: "<map>" "<name>" "<loadout>"
		while(( pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ))) != NULL )
		{
			entry_t e;
			memset( &e, 0, sizeof( e ));
			Q_strncpy( e.map, token, sizeof( e.map ));

			if(!( pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ))))
				break;
			Q_strncpy( e.name, token, sizeof( e.name ));

			if(!( pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ))))
				break;
			Q_strncpy( e.loadout, token, sizeof( e.loadout ));

			m_Chapters.AddToTail( e );
		}

		EngFuncs::COM_FreeFile( afile );
	}

	m_iSel = bound( 0, m_iSel, Q_max( 0, m_Chapters.Count() - 1 ));
	Select( m_iSel, false );
}

void CMenuContChapters::Select( int i, bool playSound )
{
	if( !m_Chapters.IsValidIndex( i ))
	{
		m_Shot.ForceUnload();
		return;
	}

	m_iSel = i;
	EnsureVisible();

	// per-chapter thumbnail, captured by hand for now (see the deferred
	// "screenshot-each-map" feature in Notes.md); placeholder otherwise
	m_Shot.ForceUnload();
	char path[256];
	snprintf( path, sizeof( path ), "gfx/shell/continuum/chapters/%s_%s.png",
		gMenu.m_gameinfo.gamefolder, m_Chapters[i].map );
	if( EngFuncs::FileExists( path, false ))
		m_Shot.Load( path );
	else
		m_Shot.Load( "gfx/shell/continuum/chapters/placeholder.png" );

	if( playSound )
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
}

void CMenuContChapters::Activate()
{
	if( !m_Chapters.IsValidIndex( m_iSel ))
		return;

	if( CL_IsActive() && gpGlobals->maxClients < 2 )
	{
		// don't silently throw away a running singleplayer game
		dialog.SetMessage( L( "StringsList_240" )); // "Starting a new game will exit any current game..."
		dialog.onPositive = VoidCb( &CMenuContChapters::StartChapter );
		dialog.Show();
	}
	else
		StartChapter();
}

void CMenuContChapters::StartChapter()
{
	if( !m_Chapters.IsValidIndex( m_iSel ))
		return;

	const entry_t &e = m_Chapters[m_iSel];

	// the game DLL grants this loadout on the player's first spawn, then clears
	// the cvar (see hlsdk). Empty means "give nothing" - the map's own scripting
	// (e.g. the c1a0 suit) takes over
	EngFuncs::CvarSetString( "sv_chapter_loadout", e.loadout );

	// singleplayer defaults (mirrors CMenuContGamePage::StartNewGame); skill is
	// left at whatever New Game last set so the player's choice carries over
	EngFuncs::CvarSetValue( "deathmatch", 0.0f );
	EngFuncs::CvarSetValue( "teamplay", 0.0f );
	EngFuncs::CvarSetValue( "pausable", 1.0f );
	EngFuncs::CvarSetValue( "coop", 0.0f );
	EngFuncs::CvarSetValue( "maxplayers", 1.0f );

	EngFuncs::PlayBackgroundTrack( NULL, NULL );
	EngFuncs::ClientCmdF( false, "newgame \"%s\"\n", e.map );
	UI_CloseMenu();
}

void CMenuContChapters::EnsureVisible()
{
	const int view = CONTENT_BOTTOM - CONTENT_TOP;
	const int rowH = ROW_H + ROW_GAP;
	const int y = m_iSel * rowH;

	if( y - m_flScrollTarget < 6 )
		m_flScrollTarget = Q_max( 0, y - 6 );
	else if( y + ROW_H - m_flScrollTarget > view - 6 )
		m_flScrollTarget = y + ROW_H - view + 6;
}

int CMenuContChapters::RowAtCursor()
{
	FOR_EACH_VEC( m_Chapters, i )
	{
		int x = MARGIN, y = CONTENT_TOP + i * ( ROW_H + ROW_GAP ) - (int)m_flScroll;
		int w = ListW(), h = ROW_H;
		UI_ScaleCoords( x, y, w, h );
		if( UI_CursorInRect( x, y, w, h ))
			return i;
	}
	return -1;
}

bool CMenuContChapters::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ))
	{
		Hide();
		return true;
	}

	if( UI::Key::IsUpArrow( key ) || UI::Key::IsDownArrow( key ))
	{
		const int next = m_iSel + ( UI::Key::IsDownArrow( key ) ? 1 : -1 );
		if( m_Chapters.IsValidIndex( next ))
			Select( next );
		return true;
	}

	if( key == K_MWHEELUP || key == K_MWHEELDOWN )
	{
		const int view = CONTENT_BOTTOM - CONTENT_TOP;
		const float maxScroll = Q_max( 0, m_Chapters.Count() * ( ROW_H + ROW_GAP ) - view );
		m_flScrollTarget = bound( 0.0f, m_flScrollTarget + ( key == K_MWHEELDOWN ? 90.0f : -90.0f ), maxScroll );
		return true;
	}

	if( UI::Key::IsEnter( key ))
	{
		Activate();
		return true;
	}

	if( key == K_MOUSE1 )
	{
		const int legendKey = Cont::LegendClickKey();
		if( legendKey )
			return KeyDown( legendKey );

		const int hit = RowAtCursor();
		if( hit >= 0 )
		{
			if( hit == m_iSel )
				m_bMousePressed = true;
			else
				Select( hit );
		}
		return true;
	}

	return CMenuFramework::KeyDown( key );
}

bool CMenuContChapters::KeyUp( int key )
{
	if( key == K_MOUSE1 )
	{
		const bool was = m_bMousePressed;
		m_bMousePressed = false;
		if( was && RowAtCursor() == m_iSel )
		{
			Activate();
			return true;
		}
	}
	return CMenuFramework::KeyUp( key );
}

bool CMenuContChapters::MouseMove( int x, int y )
{
	const int hit = RowAtCursor();
	if( hit >= 0 && hit != m_iSel )
		Select( hit );
	return true;
}

void CMenuContChapters::Draw()
{
	static CImage noBackdrop;
	// panel behind the list only; the thumbnail on the right keeps its own bg
	DrawScreenBackdrop( noBackdrop, MARGIN - 30, ListW() + 30 );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	char sub[96];
	snprintf( sub, sizeof( sub ), "%s", gMenu.m_gameinfo.title );
	for( char *p = sub; *p; p++ )
		*p = toupper( *p );

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"CHAPTERS", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		sub, clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// smooth scroll
	const float dt = gpGlobals->frametime;
	m_flScroll += ( m_flScrollTarget - m_flScroll ) * bound( 0.0f, dt * 14.0f, 1.0f );

	const int listW = ListW();
	const int clipTop = ( CONTENT_TOP + uiStatic.yOffset ) * uiStatic.scaleY;
	const int clipBottom = clipTop + ( CONTENT_BOTTOM - CONTENT_TOP ) * uiStatic.scaleY;
	EngFuncs::PIC_EnableScissor( 0, clipTop, ScreenWidth, clipBottom - clipTop );

	FOR_EACH_VEC( m_Chapters, i )
	{
		const entry_t &e = m_Chapters[i];
		const bool sel = ( i == m_iSel );

		int rx = MARGIN, ry = CONTENT_TOP + i * ( ROW_H + ROW_GAP ) - (int)m_flScroll;
		int rw = listW, rh = ROW_H;
		UI_ScaleCoords( rx, ry, rw, rh );

		if( ry + rh * 0.6f < clipTop || ry + rh * 0.4f > clipBottom )
			continue;

		if( sel )
		{
			UI_FillRect( rx, ry, rw, rh, clrAccentSoft );
			UI_FillRect( rx, ry, 3 * uiStatic.scaleX, rh, clrAccent );
		}

		const int nameH = 17 * uiStatic.scaleY;
		const int mapH = 12 * uiStatic.scaleY;

		UI_DrawString( fontItem, rx + 22 * uiStatic.scaleX, ry + ( rh - nameH ) / 2,
			rw - ( 90 + 44 ) * uiStatic.scaleX, nameH * 1.45f, e.name,
			sel ? clrInk : clrInkDim, nameH, QM_LEFT,
			ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );

		UI_DrawString( fontHint, rx + rw - 90 * uiStatic.scaleX, ry + ( rh - mapH ) / 2,
			80 * uiStatic.scaleX, mapH * 1.45f, e.map, sel ? clrInkDim : clrInkFaint, mapH, QM_RIGHT,
			ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );
	}

	EngFuncs::PIC_DisableScissor();

	if( !m_Chapters.Count( ))
	{
		const int msgH = 14 * uiStatic.scaleY;
		UI_DrawString( fontBody, MARGIN * uiStatic.scaleX, ScreenHeight * 0.45f, listW * uiStatic.scaleX, msgH * 1.45f,
			"No chapters for this game", clrInkDim, msgH, QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL );
	}

	// thumbnail panel
	{
		const int px = ( uiStatic.width - MARGIN - PANEL_W ) * uiStatic.scaleX;
		const int py = ( CONTENT_TOP + uiStatic.yOffset ) * uiStatic.scaleY;
		const int pw = PANEL_W * uiStatic.scaleX;
		const int shotH = ( PANEL_W * 3 / 4 ) * uiStatic.scaleY; // 4:3

		UI_FillRect( px - 2, py - 2, pw + 4, shotH + 4, 0x96000000 );
		if( m_Shot.IsValid( ))
			DrawPicAspectFit( px, py, pw, shotH, m_Shot );
		UI_DrawRectangleExt( px, py, pw, shotH, 0x23FFFFFF, 1 );

		if( m_Chapters.IsValidIndex( m_iSel ))
		{
			const entry_t &e = m_Chapters[m_iSel];
			const int lineH = 16 * uiStatic.scaleY;
			UI_DrawString( fontBody, px, py + shotH + 18 * uiStatic.scaleY, pw, lineH * 1.45f,
				e.name, clrInk, lineH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );
		}
	}

	CMenuFramework::Draw(); // dialog

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Start" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ));
}

static CMenuContChapters *s_pChapters = NULL;

static void UI_ContChapters_Precache( void )
{
	s_pChapters = new CMenuContChapters();
}

static void UI_ContChapters_Shutdown( void )
{
	delete s_pChapters;
	s_pChapters = NULL;
}

void UI_ContChapters_Menu( void )
{
	if( gMenu.m_gameinfo.gamemode == GAME_MULTIPLAYER_ONLY )
		return;

	if( !EngFuncs::CheckGameDll( ))
		return;

	s_pChapters->Show();
}

ADD_MENU4( menu_continuum_chapters, UI_ContChapters_Precache, UI_ContChapters_Menu, UI_ContChapters_Shutdown );
