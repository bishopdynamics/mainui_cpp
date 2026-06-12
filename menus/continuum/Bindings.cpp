/*
Bindings.cpp -- Continuum keyboard & mouse bindings screen
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
#include "Continuum.h"
#include "FontManager.h"
#include "YesNoMessageBox.h"
#include "keydefs.h"
#include "KbActListModel.h"

using namespace Cont;

#define ROW_W       760
#define ROW_H       44
#define HEADER_H    40
#define CONTENT_TOP 208
#define CONTENT_BOTTOM ( 768 - LEGEND_H - 22 )

// key name columns, right-aligned inside the row
#define KEY1_X      ( ROW_W - 290 )
#define KEY2_X      ( ROW_W - 130 )
#define KEYCOL_W    150

class CMenuContBindings : public CMenuFramework
{
public:
	CMenuContBindings() : CMenuFramework( "CMenuContBindings" ),
		model( CMenuKbActListModel::VIEW_BINDINGS ),
		m_iSel( 0 ), m_iTotalH( 0 ), m_flScroll( 0 ), m_flScrollTarget( 0 ),
		m_bGrab( false ), m_bMousePressed( false ) { }

	bool KeyDown( int key ) override;
	bool KeyUp( int key ) override;
	bool MouseMove( int x, int y ) override;
	void Draw() override;
	void Show() override;
	void Hide() override;

private:
	void _Init() override;
	void _VidInit() override;

	struct row_t
	{
		char label[96];
		char bind[64];   // empty = section header
		char key1[24];
		char key2[24];
		int key1num;     // raw keynums for glyph lookup, -1 = unbound
		int key2num;
		int baseY;       // logical y inside the list, before scroll
		int height;
	};

	void Refresh();           // re-read kb_act.lst + current binds
	void Select( int i, bool playSound = true );
	void MoveSel( int dir );
	void JumpSection( int dir );
	void EnterGrab();
	void BindGrabbedKey( int key );
	void ClearSelected();
	void AskResetDefaults();
	void ResetDefaults();
	void EnsureVisible();
	int RowAtCursor();        // render-space hit test, -1 if none

	static void StripColors( const char *in, char *out, size_t size );
	static void UnbindCommand( const char *command );

	CMenuKbActListModel model;
	CUtlVector<row_t> m_Rows;
	int m_iSel;
	int m_iTotalH;
	float m_flScroll, m_flScrollTarget;
	bool m_bGrab;
	bool m_bMousePressed;
	CMenuYesNoMessageBox dialog;
};

void CMenuContBindings::StripColors( const char *in, char *out, size_t size )
{
	size_t o = 0;
	for( ; *in && o < size - 1; in++ )
	{
		if( in[0] == '^' && in[1] >= '0' && in[1] <= '9' )
		{
			in++;
			continue;
		}
		out[o++] = *in;
	}
	out[o] = 0;
}

void CMenuContBindings::UnbindCommand( const char *command )
{
	const size_t len = strlen( command );

	for( int i = 0; ; i++ )
	{
		const char *str = EngFuncs::KeynumToString( i );
		if( !strcmp( str, "<OUT OF RANGE>" ))
			break;

		const char *b = EngFuncs::KEY_GetBinding( i );
		if( b && !strncmp( b, command, len ))
			EngFuncs::KEY_SetBinding( i, "" );
	}
}

void CMenuContBindings::Refresh()
{
	model.Update();
	m_Rows.RemoveAll();

	int y = 0;
	FOR_EACH_VEC( model.entries, i )
	{
		const CMenuKbActListModel::entry_t &e = model.entries[i];

		// kb_act.lst uses runs of "=====" rows around section names; drop the
		// decoration rows, our headers carry the separation on their own
		char label[96];
		StripColors( e.display, label, sizeof( label ));
		if( !e.bind[0] && label[0] == '=' )
			continue;

		row_t r;
		Q_strncpy( r.label, label, sizeof( r.label ));
		Q_strncpy( r.bind, e.bind, sizeof( r.bind ));
		StripColors( e.first, r.key1, sizeof( r.key1 ));
		StripColors( e.second, r.key2, sizeof( r.key2 ));

		r.key1num = r.key2num = -1;
		if( e.bind[0] )
		{
			int keys[2];
			CMenuKbActListModel::LookupBoundKeys( e.bind, keys );
			r.key1num = keys[0];
			r.key2num = keys[1];
		}

		r.height = e.bind[0] ? ROW_H : HEADER_H;
		r.baseY = y;
		y += r.height + 4;

		m_Rows.AddToTail( r );
	}
	m_iTotalH = y;

	if( !m_Rows.IsValidIndex( m_iSel ) || !m_Rows[m_iSel].bind[0] )
	{
		m_iSel = 0;
		FOR_EACH_VEC( m_Rows, i )
		{
			if( m_Rows[i].bind[0] )
			{
				m_iSel = i;
				break;
			}
		}
	}
}

void CMenuContBindings::_Init()
{
	dialog.SetMessage( "Reset all bindings to the defaults?" );
	dialog.onPositive = VoidCb( &CMenuContBindings::ResetDefaults );
	dialog.Link( this );
}

void CMenuContBindings::_VidInit()
{
	VidInitFonts();
}

void CMenuContBindings::Show()
{
	CMenuFramework::Show();
	m_bGrab = false;
	Refresh();
}

void CMenuContBindings::Hide()
{
	// bindings are part of the unified config shared by all games
	EngFuncs::ClientCmd( false, "host_writeconfig\n" );
	CMenuFramework::Hide();
}

void CMenuContBindings::Select( int i, bool playSound )
{
	if( !m_Rows.IsValidIndex( i ) || !m_Rows[i].bind[0] || i == m_iSel )
		return;

	m_iSel = i;
	EnsureVisible();
	if( playSound )
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
}

void CMenuContBindings::MoveSel( int dir )
{
	for( int i = m_iSel + dir; i >= 0 && i < m_Rows.Count(); i += dir )
	{
		if( m_Rows[i].bind[0] )
		{
			Select( i );
			return;
		}
	}
}

// LB/RB: jump to the first action of the previous/next section
void CMenuContBindings::JumpSection( int dir )
{
	int i = m_iSel;

	// walk to the header bounding the current section in this direction
	while( i >= 0 && i < m_Rows.Count() && m_Rows[i].bind[0] )
		i += dir;
	// going backwards, skip past the section's own header to the previous one
	if( dir < 0 )
	{
		i--;
		while( i >= 0 && m_Rows[i].bind[0] )
			i--;
	}

	// then to the first action after that header
	for( i += 1; i >= 0 && i < m_Rows.Count(); i++ )
	{
		if( m_Rows[i].bind[0] )
		{
			Select( i );
			return;
		}
	}
}

void CMenuContBindings::EnsureVisible()
{
	if( !m_Rows.IsValidIndex( m_iSel ))
		return;

	const row_t &r = m_Rows[m_iSel];
	const int view = CONTENT_BOTTOM - CONTENT_TOP;

	if( r.baseY - m_flScrollTarget < 6 )
		m_flScrollTarget = Q_max( 0, r.baseY - 6 );
	else if( r.baseY + r.height - m_flScrollTarget > view - 6 )
		m_flScrollTarget = r.baseY + r.height - view + 6;
}

void CMenuContBindings::EnterGrab()
{
	if( !m_Rows.IsValidIndex( m_iSel ) || !m_Rows[m_iSel].bind[0] )
		return;

	m_bGrab = true;
	EngFuncs::PlayLocalSound( uiStatic.sounds[SND_KEY] );
}

void CMenuContBindings::BindGrabbedKey( int key )
{
	m_bGrab = false;

	if( UI::Key::IsEscape( key ))
	{
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_OUT] );
		Refresh();
		return;
	}

	if( UI::Key::IsConsole( key ))
	{
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_BUZZ] );
		Refresh();
		return;
	}

	// both slots already taken by other keys: this binding replaces them.
	// Only now — cancelling the grab must never touch the existing keys
	int keys[2];
	CMenuKbActListModel::LookupBoundKeys( m_Rows[m_iSel].bind, keys );
	if( keys[1] != -1 && key != keys[0] && key != keys[1] )
		UnbindCommand( m_Rows[m_iSel].bind );

	EngFuncs::ClientCmdF( true, "bind \"%s\" \"%s\"\n",
		EngFuncs::KeynumToString( key ), m_Rows[m_iSel].bind );
	EngFuncs::PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
	Refresh();
}

void CMenuContBindings::ClearSelected()
{
	if( !m_Rows.IsValidIndex( m_iSel ) || !m_Rows[m_iSel].bind[0] )
		return;

	UnbindCommand( m_Rows[m_iSel].bind );
	EngFuncs::PlayLocalSound( uiStatic.sounds[SND_REMOVEKEY] );
	Refresh();
}

void CMenuContBindings::AskResetDefaults()
{
	dialog.Show();
}

void CMenuContBindings::ResetDefaults()
{
	char *afile = (char *)EngFuncs::COM_LoadFile( "gfx/shell/kb_def.lst", NULL );
	char *pfile = afile;
	char token[1024];

	if( !afile )
		return;

	EngFuncs::ClientCmd( true, "unbindall" );

	while(( pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ))) != NULL )
	{
		char key[32];
		Q_strncpy( key, token, sizeof( key ));

		pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ));
		if( !pfile )
			break;

		if( key[0] == '\\' && key[1] == '\\' )
		{
			key[0] = '\\';
			key[1] = '\0';
		}

		EngFuncs::ClientCmdF( true, "bind \"%s\" \"%s\"\n", key, token );
	}

	EngFuncs::COM_FreeFile( afile );
	EngFuncs::PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
	Refresh();
}

int CMenuContBindings::RowAtCursor()
{
	const int clipTop = ( CONTENT_TOP + uiStatic.yOffset ) * uiStatic.scaleY;
	const int clipBottom = clipTop + ( CONTENT_BOTTOM - CONTENT_TOP ) * uiStatic.scaleY;

	if( uiStatic.cursorY < clipTop || uiStatic.cursorY > clipBottom )
		return -1;

	FOR_EACH_VEC( m_Rows, i )
	{
		if( !m_Rows[i].bind[0] )
			continue;

		int x = MARGIN, y = CONTENT_TOP + m_Rows[i].baseY - (int)m_flScroll;
		int w = ROW_W, h = m_Rows[i].height;
		UI_ScaleCoords( x, y, w, h );
		if( UI_CursorInRect( x, y, w, h ))
			return i;
	}
	return -1;
}

bool CMenuContBindings::KeyDown( int key )
{
	if( m_bGrab )
		return true; // the binding happens on KeyUp

	if( UI::Key::IsEscape( key ))
	{
		Hide();
		return true;
	}

	if( UI::Key::IsUpArrow( key ))
	{
		MoveSel( -1 );
		return true;
	}
	if( UI::Key::IsDownArrow( key ))
	{
		MoveSel( 1 );
		return true;
	}

	if( UI::Key::IsPageUp( key ))
	{
		JumpSection( -1 );
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
		return true;
	}
	if( UI::Key::IsPageDown( key ))
	{
		JumpSection( 1 );
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
		return true;
	}

	if( key == K_MWHEELUP || key == K_MWHEELDOWN )
	{
		const int view = CONTENT_BOTTOM - CONTENT_TOP;
		const float maxScroll = Q_max( 0, m_iTotalH - view );
		m_flScrollTarget = bound( 0.0f, m_flScrollTarget + ( key == K_MWHEELDOWN ? 90.0f : -90.0f ), maxScroll );
		return true;
	}

	if( key == K_X_BUTTON || key == 'x' )
	{
		ClearSelected();
		return true;
	}

	if( key == K_Y_BUTTON || key == 'y' )
	{
		AskResetDefaults();
		return true;
	}

	if( UI::Key::IsEnter( key ))
		return true; // grab starts on the release

	if( key == K_MOUSE1 )
	{
		const int legendKey = Cont::LegendClickKey();
		if( legendKey )
			return KeyDown( legendKey );

		const int hit = RowAtCursor();
		if( hit >= 0 )
		{
			Select( hit );
			m_bMousePressed = true;
		}
		return true;
	}

	return CMenuFramework::KeyDown( key );
}

bool CMenuContBindings::KeyUp( int key )
{
	if( m_bGrab )
	{
		BindGrabbedKey( key );
		return true;
	}

	if( UI::Key::IsEnter( key ))
	{
		EnterGrab();
		return true;
	}

	if( key == K_MOUSE1 )
	{
		const bool wasPressed = m_bMousePressed;
		m_bMousePressed = false;
		if( wasPressed && RowAtCursor() == m_iSel )
		{
			EnterGrab();
			return true;
		}
	}

	return CMenuFramework::KeyUp( key );
}

bool CMenuContBindings::MouseMove( int x, int y )
{
	if( !m_bGrab )
	{
		const int hit = RowAtCursor();
		if( hit >= 0 )
			Select( hit );
	}
	return true;
}

void CMenuContBindings::Draw()
{
	static CImage noBackdrop;
	DrawBackdrop( noBackdrop );

	// title block
	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"INPUT BINDINGS", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		"KEYBOARD, MOUSE & GAMEPAD - SHARED BY ALL GAMES", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// column captions above the key columns
	const int capH = 11 * uiStatic.scaleY;
	const int capY = ( CONTENT_TOP - 24 ) * uiStatic.scaleY + uiStatic.yOffset * uiStatic.scaleY;
	UI_DrawString( fontSmall, ( MARGIN + KEY1_X ) * uiStatic.scaleX, capY, KEYCOL_W * uiStatic.scaleX, capH * 1.45f,
		"KEY", clrInkFaint, capH, QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, ( MARGIN + KEY2_X ) * uiStatic.scaleX, capY, KEYCOL_W * uiStatic.scaleX, capH * 1.45f,
		"ALT", clrInkFaint, capH, QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// smooth scroll
	const float dt = gpGlobals->frametime;
	m_flScroll += ( m_flScrollTarget - m_flScroll ) * bound( 0.0f, dt * 14.0f, 1.0f );

	// rows, clipped to the content viewport
	const int clipTop = ( CONTENT_TOP + uiStatic.yOffset ) * uiStatic.scaleY;
	const int clipBottom = clipTop + ( CONTENT_BOTTOM - CONTENT_TOP ) * uiStatic.scaleY;
	EngFuncs::PIC_EnableScissor( 0, clipTop, ScreenWidth, clipBottom - clipTop );

	FOR_EACH_VEC( m_Rows, i )
	{
		const row_t &r = m_Rows[i];

		int x = MARGIN, y = CONTENT_TOP + r.baseY - (int)m_flScroll;
		int w = ROW_W, h = r.height;
		UI_ScaleCoords( x, y, w, h );

		// FillRGBA ignores the scissor: cull rows mostly outside the viewport
		if( y + h * 0.6f < clipTop || y + h * 0.4f > clipBottom )
			continue;

		if( !r.bind[0] )
		{
			// section header
			const int hh = 12 * uiStatic.scaleY;
			UI_DrawString( fontSmall, x + 6 * uiStatic.scaleX, y + h - hh * 1.6f, w, hh * 1.45f,
				r.label, clrAccent, hh, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
			continue;
		}

		const bool sel = ( i == m_iSel );

		if( sel )
		{
			UI_FillRect( x, y, w, h, clrAccentSoft );
			UI_FillRect( x, y, 3 * uiStatic.scaleX, h, clrAccent );
		}

		const int labelH = 16 * uiStatic.scaleY;
		UI_DrawString( fontItem, x + 22 * uiStatic.scaleX, y + ( h - labelH ) / 2, KEY1_X * uiStatic.scaleX,
			labelH * 1.45f, r.label, sel ? clrInk : clrInkDim, labelH, QM_LEFT,
			ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );

		// bound keys: primary bright, alternate dim; pad buttons draw as the
		// controller glyph the rest of the UI uses
		const int keyH = 14 * uiStatic.scaleY;
		const int glyphH = 30 * uiStatic.scaleY;
		const struct { const char *text; int keynum; bool bright; int colX; } cols[2] =
		{
			{ r.key1, r.key1num, sel, KEY1_X },
			{ r.key2, r.key2num, false, KEY2_X },
		};

		for( int c = 0; c < 2; c++ )
		{
			const int cx = x + cols[c].colX * uiStatic.scaleX;
			const int colW = KEYCOL_W * uiStatic.scaleX;
			const EGlyph g = cols[c].keynum >= 0 ? KeyToGlyph( cols[c].keynum ) : GLYPH_COUNT;
			const int gw = GlyphWidth( g, glyphH );

			if( gw )
			{
				DrawGlyph( g, cx + ( colW - gw ) / 2, y + ( h - glyphH ) / 2, glyphH );
				continue;
			}

			const char *text = cols[c].text[0] ? cols[c].text : "-";
			unsigned int color = !cols[c].text[0] ? clrInkFaint : ( cols[c].bright ? clrInk : clrInkDim );
			UI_DrawString( fontBody, cx, y + ( h - keyH ) / 2, colW, keyH * 1.45f,
				text, color, keyH, QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );
		}
	}

	EngFuncs::PIC_DisableScissor();

	CMenuFramework::Draw(); // the reset-confirm dialog, when visible

	// grab-mode overlay above everything
	if( m_bGrab )
	{
		UI_FillRect( 0, 0, ScreenWidth, ScreenHeight, 0x96000000 );

		const int pw = 620 * uiStatic.scaleX;
		const int ph = 150 * uiStatic.scaleY;
		const int px = ( ScreenWidth - pw ) / 2;
		const int py = ( ScreenHeight - ph ) / 2;

		UI_FillRect( px, py, pw, ph, 0xF20E1014 );
		UI_DrawRectangleExt( px, py, pw, ph, 0x28FFFFFF, 1 );
		UI_FillRect( px, py, pw, 3 * uiStatic.scaleY, clrAccent );

		const int lineH = 17 * uiStatic.scaleY;
		UI_DrawString( fontItem, px, py + 36 * uiStatic.scaleY, pw, lineH * 1.45f,
			m_Rows.IsValidIndex( m_iSel ) ? m_Rows[m_iSel].label : "", clrInk, lineH, QM_CENTER,
			ETF_NOSIZELIMIT | ETF_FORCECOL );

		const int hintH = 13 * uiStatic.scaleY;
		UI_DrawString( fontHint, px, py + 80 * uiStatic.scaleY, pw, hintH * 1.45f,
			"Press a key or button - Esc cancels", clrInkDim, hintH, QM_CENTER,
			ETF_NOSIZELIMIT | ETF_FORCECOL );
		return;
	}

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Rebind" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
		{ GLYPH_LB, GLYPH_RB, "Section" },
		{ GLYPH_X, GLYPH_COUNT, "Clear" },
		{ GLYPH_Y, GLYPH_COUNT, "Defaults" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ));
}

ADD_MENU( menu_continuum_bindings, CMenuContBindings, UI_ContBindings_Menu );
