/*
Saves.cpp -- Continuum load/save screen: save list + preview panel
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

using namespace Cont;

#define ROW_H       52
#define CONTENT_TOP 208
#define CONTENT_BOTTOM ( 768 - LEGEND_H - 22 )
#define PANEL_W     560   // preview panel on the right

static int COM_CompareSaves( const void *a, const void *b )
{
	const char *file1 = *((const char **)a);
	const char *file2 = *((const char **)b);
	int bResult = 0;

	EngFuncs::CompareFileTime( file2, file1, &bResult );
	return bResult;
}

class CMenuContSaves : public CMenuFramework
{
public:
	CMenuContSaves() : CMenuFramework( "CMenuContSaves" ),
		m_bSaveMode( false ), m_iSel( 0 ), m_flScroll( 0 ), m_flScrollTarget( 0 ) { }

	void SetSaveMode( bool save ) { m_bSaveMode = save; }

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
		char name[CS_SIZE];   // filename base; "new" = the magic new-save slot
		char title[256];
		char date[CS_SIZE];
		char elapsed[CS_SIZE];
	};

	void Refresh();
	void Select( int i, bool playSound = true );
	void Activate();      // load or save the selection
	void DoSave();
	void DoDelete();
	void EnsureVisible();
	int RowAtCursor();
	int ListW() const { return uiStatic.width - 2 * MARGIN - PANEL_W - 24; }
	bool IsNewRow( int i ) const { return m_bSaveMode && i == 0; }

	CUtlVector<entry_t> m_Saves;
	CImage m_Shot;
	bool m_bSaveMode;
	int m_iSel;
	float m_flScroll, m_flScrollTarget;
	bool m_bMousePressed = false;

	CMenuYesNoMessageBox deleteDialog;
	CMenuYesNoMessageBox overwriteDialog;
};

void CMenuContSaves::_Init()
{
	deleteDialog.SetMessage( "Delete this saved game?" );
	deleteDialog.onPositive = VoidCb( &CMenuContSaves::DoDelete );
	deleteDialog.Link( this );

	overwriteDialog.SetMessage( "Overwrite this saved game?" );
	overwriteDialog.onPositive = VoidCb( &CMenuContSaves::DoSave );
	overwriteDialog.Link( this );
}

void CMenuContSaves::_VidInit()
{
	VidInitFonts();
}

void CMenuContSaves::Show()
{
	CMenuFramework::Show();
	m_iSel = 0;
	m_flScroll = m_flScrollTarget = 0;
	Refresh();
}

void CMenuContSaves::Refresh()
{
	char **filenames;
	int numFiles;

	m_Saves.RemoveAll();

	filenames = EngFuncs::GetFilesList( "save/*.sav", &numFiles, true );
	qsort( filenames, numFiles, sizeof( *filenames ), COM_CompareSaves );

	if( m_bSaveMode && CL_IsActive( ))
	{
		entry_t e;
		memset( &e, 0, sizeof( e ));
		Q_strncpy( e.name, "new", sizeof( e.name )); // handled by SV_Save_f
		Q_strncpy( e.title, "New Save", sizeof( e.title ));
		Q_strncpy( e.date, "Current game", sizeof( e.date ));
		m_Saves.AddToTail( e );
	}

	for( int i = 0; i < numFiles; i++ )
	{
		entry_t e;
		char comment[256];

		memset( &e, 0, sizeof( e ));
		COM_FileBase( filenames[i], e.name, sizeof( e.name ));

		if( !EngFuncs::GetSaveComment( filenames[i], comment ))
		{
			// engine marks broken saves in the comment (<CORRUPTED> etc.)
			if( comment[0] )
			{
				Q_strncpy( e.title, comment, sizeof( e.title ));
				m_Saves.AddToTail( e );
			}
			continue;
		}

		// fixed-layout comment string: title, time, date, elapsed time
		char time[CS_TIME], date[CS_TIME];
		Q_strncpy( time, comment + CS_SIZE, CS_TIME );
		Q_strncpy( date, comment + CS_SIZE + CS_TIME, CS_TIME );
		snprintf( e.date, sizeof( e.date ), "%s %s", date, time );
		Q_strncpy( e.elapsed, comment + CS_SIZE + CS_TIME * 2, sizeof( e.elapsed ));

		// "[quick]Title" / "[autosave]Title" / translatable "#token"
		char *title = comment, *type = NULL, *p = NULL;
		if( comment[0] == '[' && ( p = strchr( comment, ']' )))
		{
			type = comment + 1;
			title = p + 1;
		}

		if( title[0] == '#' )
		{
			char s[CS_SIZE];

			if( p )
				*p = 0;
			if(( p = strchr( title, ' ' )))
				*p = 0;

			Q_strncpy( s, title, sizeof( s ));
			if( type )
				snprintf( e.title, sizeof( e.title ), "[%.16s] %s", type, L( s ));
			else
				Q_strncpy( e.title, L( s ), sizeof( e.title ));
		}
		else
		{
			for( int len = (int)strlen( title ) - 1; len >= 0 && isspace( title[len] ); len-- )
				title[len] = 0;
			Q_strncpy( e.title, comment, sizeof( e.title ));
		}

		m_Saves.AddToTail( e );
	}

	m_iSel = bound( 0, m_iSel, Q_max( 0, m_Saves.Count() - 1 ));
	Select( m_iSel, false );
}

void CMenuContSaves::Select( int i, bool playSound )
{
	if( !m_Saves.IsValidIndex( i ))
	{
		m_Shot.ForceUnload();
		return;
	}

	m_iSel = i;
	EnsureVisible();

	m_Shot.ForceUnload();
	if( !IsNewRow( i ))
	{
		char path[128];
		snprintf( path, sizeof( path ), "save/%s.bmp", m_Saves[i].name );
		if( EngFuncs::FileExists( path, true ))
			m_Shot.Load( path );
	}

	if( playSound )
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
}

void CMenuContSaves::Activate()
{
	if( !m_Saves.IsValidIndex( m_iSel ))
		return;

	if( m_bSaveMode )
	{
		if( IsNewRow( m_iSel ))
			DoSave();
		else
			overwriteDialog.Show();
		return;
	}

	EngFuncs::StopBackgroundTrack();
	EngFuncs::ClientCmdF( false, "load \"%s\"\n", m_Saves[m_iSel].name );
	UI_CloseMenu();
}

void CMenuContSaves::DoSave()
{
	if( !m_Saves.IsValidIndex( m_iSel ))
		return;

	// the engine reuses the shot name; drop the cached pic first
	char path[128];
	snprintf( path, sizeof( path ), "save/%s.bmp", m_Saves[m_iSel].name );
	EngFuncs::PIC_Free( path );

	EngFuncs::ClientCmdF( false, "save \"%s\"\n", m_Saves[m_iSel].name );
	UI_CloseMenu();
}

void CMenuContSaves::DoDelete()
{
	if( !m_Saves.IsValidIndex( m_iSel ) || IsNewRow( m_iSel ))
		return;

	char path[128];
	EngFuncs::ClientCmdF( true, "killsave \"%s\"\n", m_Saves[m_iSel].name );
	snprintf( path, sizeof( path ), "save/%s.bmp", m_Saves[m_iSel].name );
	EngFuncs::PIC_Free( path );

	EngFuncs::PlayLocalSound( uiStatic.sounds[SND_REMOVEKEY] );
	Refresh();
}

void CMenuContSaves::EnsureVisible()
{
	const int view = CONTENT_BOTTOM - CONTENT_TOP;
	const int rowH = ROW_H + 4;
	const int y = m_iSel * rowH;

	if( y - m_flScrollTarget < 6 )
		m_flScrollTarget = Q_max( 0, y - 6 );
	else if( y + ROW_H - m_flScrollTarget > view - 6 )
		m_flScrollTarget = y + ROW_H - view + 6;
}

int CMenuContSaves::RowAtCursor()
{
	FOR_EACH_VEC( m_Saves, i )
	{
		int x = MARGIN, y = CONTENT_TOP + i * ( ROW_H + 4 ) - (int)m_flScroll;
		int w = ListW(), h = ROW_H;
		UI_ScaleCoords( x, y, w, h );
		if( UI_CursorInRect( x, y, w, h ))
			return i;
	}
	return -1;
}

bool CMenuContSaves::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ))
	{
		Hide();
		return true;
	}

	if( UI::Key::IsUpArrow( key ) || UI::Key::IsDownArrow( key ))
	{
		const int next = m_iSel + ( UI::Key::IsDownArrow( key ) ? 1 : -1 );
		if( m_Saves.IsValidIndex( next ))
			Select( next );
		return true;
	}

	if( key == K_MWHEELUP || key == K_MWHEELDOWN )
	{
		const int view = CONTENT_BOTTOM - CONTENT_TOP;
		const float maxScroll = Q_max( 0, m_Saves.Count() * ( ROW_H + 4 ) - view );
		m_flScrollTarget = bound( 0.0f, m_flScrollTarget + ( key == K_MWHEELDOWN ? 90.0f : -90.0f ), maxScroll );
		return true;
	}

	if( key == K_X_BUTTON || key == 'x' )
	{
		if( m_Saves.IsValidIndex( m_iSel ) && !IsNewRow( m_iSel ))
			deleteDialog.Show();
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

bool CMenuContSaves::KeyUp( int key )
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

bool CMenuContSaves::MouseMove( int x, int y )
{
	const int hit = RowAtCursor();
	if( hit >= 0 && hit != m_iSel )
		Select( hit );
	return true;
}

void CMenuContSaves::Draw()
{
	static CImage noBackdrop;
	// panel behind the list only; the screenshot preview on the right keeps its own bg
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
		m_bSaveMode ? "SAVE GAME" : "LOAD GAME", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		sub, clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// smooth scroll
	const float dt = gpGlobals->frametime;
	m_flScroll += ( m_flScrollTarget - m_flScroll ) * bound( 0.0f, dt * 14.0f, 1.0f );

	const int listW = ListW();
	const int clipTop = ( CONTENT_TOP + uiStatic.yOffset ) * uiStatic.scaleY;
	const int clipBottom = clipTop + ( CONTENT_BOTTOM - CONTENT_TOP ) * uiStatic.scaleY;
	EngFuncs::PIC_EnableScissor( 0, clipTop, ScreenWidth, clipBottom - clipTop );

	FOR_EACH_VEC( m_Saves, i )
	{
		const entry_t &e = m_Saves[i];
		const bool sel = ( i == m_iSel );

		int rx = MARGIN, ry = CONTENT_TOP + i * ( ROW_H + 4 ) - (int)m_flScroll;
		int rw = listW, rh = ROW_H;
		UI_ScaleCoords( rx, ry, rw, rh );

		if( ry + rh * 0.6f < clipTop || ry + rh * 0.4f > clipBottom )
			continue;

		if( sel )
		{
			UI_FillRect( rx, ry, rw, rh, clrAccentSoft );
			UI_FillRect( rx, ry, 3 * uiStatic.scaleX, rh, clrAccent );
		}

		const bool isNew = IsNewRow( i );
		const int nameH = 16 * uiStatic.scaleY;
		const int cellH = 13 * uiStatic.scaleY;

		UI_DrawString( fontItem, rx + 22 * uiStatic.scaleX, ry + ( rh - nameH ) / 2,
			rw - ( 280 + 44 ) * uiStatic.scaleX, nameH * 1.45f, e.title,
			isNew ? clrAccent : ( sel ? clrInk : clrInkDim ), nameH, QM_LEFT,
			ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );

		UI_DrawString( fontHint, rx + rw - 280 * uiStatic.scaleX, ry + ( rh - cellH ) / 2,
			260 * uiStatic.scaleX, cellH * 1.45f, e.date, sel ? clrInkDim : clrInkFaint, cellH, QM_RIGHT,
			ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );
	}

	EngFuncs::PIC_DisableScissor();

	if( !m_Saves.Count( ))
	{
		const int msgH = 14 * uiStatic.scaleY;
		UI_DrawString( fontBody, MARGIN * uiStatic.scaleX, ScreenHeight * 0.45f, listW * uiStatic.scaleX, msgH * 1.45f,
			m_bSaveMode ? "Nothing to save - start a game first" : "No saved games yet",
			clrInkDim, msgH, QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL );
	}

	// preview panel
	{
		const int px = ( uiStatic.width - MARGIN - PANEL_W ) * uiStatic.scaleX;
		const int py = ( CONTENT_TOP + uiStatic.yOffset ) * uiStatic.scaleY;
		const int pw = PANEL_W * uiStatic.scaleX;
		const int shotH = ( PANEL_W * 3 / 4 ) * uiStatic.scaleY; // saves shots are 4:3

		UI_FillRect( px - 2, py - 2, pw + 4, shotH + 4, 0x96000000 );
		if( m_Shot.IsValid( ))
			DrawPicAspectFit( px, py, pw, shotH, m_Shot );
		else
		{
			const int hintH = 13 * uiStatic.scaleY;
			UI_DrawString( fontHint, px, py + shotH / 2 - hintH / 2, pw, hintH * 1.45f,
				m_Saves.IsValidIndex( m_iSel ) && IsNewRow( m_iSel ) ? "A new beginning" : "No screenshot",
				clrInkFaint, hintH, QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL );
		}
		UI_DrawRectangleExt( px, py, pw, shotH, 0x23FFFFFF, 1 );

		if( m_Saves.IsValidIndex( m_iSel ))
		{
			const entry_t &e = m_Saves[m_iSel];
			const int lineH = 14 * uiStatic.scaleY;
			int yy = py + shotH + 18 * uiStatic.scaleY;

			UI_DrawString( fontBody, px, yy, pw, lineH * 1.45f, e.date, clrInkDim, lineH,
				QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );
			if( e.elapsed[0] )
			{
				yy += lineH * 1.6f;
				char buf[96];
				snprintf( buf, sizeof( buf ), "Time played: %s", e.elapsed );
				UI_DrawString( fontHint, px, yy, pw, lineH * 1.45f, buf, clrInkFaint, 13 * uiStatic.scaleY,
					QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );
			}
		}
	}

	CMenuFramework::Draw(); // dialogs

	static const LegendEntry loadLegend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Load" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
		{ GLYPH_X, GLYPH_COUNT, "Delete" },
	};
	static const LegendEntry saveLegend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Save" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
		{ GLYPH_X, GLYPH_COUNT, "Delete" },
	};
	if( m_bSaveMode )
		DrawLegend( saveLegend, V_ARRAYSIZE( saveLegend ));
	else
		DrawLegend( loadLegend, V_ARRAYSIZE( loadLegend ));
}

static CMenuContSaves *menu_continuum_saves = NULL;

static void UI_ContSaves_Precache( void )
{
	menu_continuum_saves = new CMenuContSaves();
}

static void UI_ContSaves_Shutdown( void )
{
	delete menu_continuum_saves;
	menu_continuum_saves = NULL;
}

static void UI_ContSaves_Menu( bool saveMode )
{
	if( gMenu.m_gameinfo.gamemode == GAME_MULTIPLAYER_ONLY )
		return;

	if( !EngFuncs::CheckGameDll( ))
		return;

	menu_continuum_saves->SetSaveMode( saveMode );
	menu_continuum_saves->Show();
}

void UI_ContLoadGame_Menu( void )
{
	UI_ContSaves_Menu( false );
}

void UI_ContSaveGame_Menu( void )
{
	UI_ContSaves_Menu( true );
}

ADD_MENU4( menu_continuum_loadgame, UI_ContSaves_Precache, UI_ContLoadGame_Menu, UI_ContSaves_Shutdown );
ADD_MENU4( menu_continuum_savegame, NULL, UI_ContSaveGame_Menu, NULL );
