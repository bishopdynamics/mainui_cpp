/*
GamePicker.cpp -- Continuum game picker: horizontal card carousel
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

using namespace Cont;

// curated metadata for the games we ship; anything else found on disk gets a
// generic card after these
struct gamedef_t
{
	const char *folder;
	const char *title;
	const char *meta;
};

static const gamedef_t g_KnownGames[] =
{
	{ "valve",   "Half-Life",      "Valve - 1998 - 96 maps" },
	{ "gearbox", "Opposing Force", "Gearbox - 1999 - 43 maps" },
	{ "bshift",  "Blue Shift",     "Gearbox - 2001 - 37 maps" },
	{ "hunger",  "They Hunger",    "Neil Manke - 2001 - 63 maps" },
	{ "uplink",  "Uplink",         "Valve - 1999 - 10 maps" },
	{ "dayone",  "Day One",        "Valve - 1998 - 27 maps" },
};

// card layout, logical units
#define CARD_W      176
#define CARD_ART_H  132
#define CARD_LBL_H  50
#define CARD_H      ( CARD_ART_H + CARD_LBL_H )
#define CARD_GAP    20
#define CARD_Y      250

class CMenuContGamePicker : public CMenuFramework
{
public:
	CMenuContGamePicker() : CMenuFramework( "CMenuContGamePicker" ), m_iCount( 0 ), m_iSel( 0 ), m_flScroll( 0 ) { }

	bool KeyDown( int key ) override;
	bool MouseMove( int x, int y ) override;
	void Draw() override;
	void Show() override;

private:
	void _Init() override;
	void _VidInit() override;

	void RefreshGames();
	void SelectCard( int i, bool playSound = true );
	void ActivateCard();
	void CardRect( int i, int &x, int &y, int &w, int &h );

	struct card_t
	{
		char folder[64];
		char title[64];
		char meta[64];
		CImage art;
		CImage backdrop;
		bool current;
	};

	enum { MAX_CARDS = 16 };
	card_t m_Cards[MAX_CARDS];
	int m_iCount;
	int m_iSel;
	float m_flScroll;       // current animated scroll, logical px
	CMenuYesNoMessageBox dialog;
};

void CMenuContGamePicker::_Init()
{
	dialog.Link( this );
}

void CMenuContGamePicker::_VidInit()
{
	VidInitFonts();
	RefreshGames();
}

void CMenuContGamePicker::Show()
{
	CMenuFramework::Show();
	RefreshGames();
}

void CMenuContGamePicker::RefreshGames()
{
	const char *currentGame = gMenu.m_gameinfo.gamefolder;

	m_iCount = 0;

	// curated games first, in our order, when actually installed
	for( size_t k = 0; k < V_ARRAYSIZE( g_KnownGames ) && m_iCount < MAX_CARDS; k++ )
	{
		for( int i = 0; ; i++ )
		{
			gameinfo2_t *gi = EngFuncs::GetModInfo( i );
			if( !gi ) break;

			if( stricmp( gi->gamefolder, g_KnownGames[k].folder ))
				continue;

			card_t &c = m_Cards[m_iCount++];
			Q_strncpy( c.folder, g_KnownGames[k].folder, sizeof( c.folder ));
			Q_strncpy( c.title, g_KnownGames[k].title, sizeof( c.title ));
			Q_strncpy( c.meta, g_KnownGames[k].meta, sizeof( c.meta ));
			GameArt( c.folder, c.art );
			GameBackdrop( c.folder, c.backdrop );
			c.current = !stricmp( currentGame, c.folder );
			break;
		}
	}

	// any other installed mods get plain cards
	for( int i = 0; m_iCount < MAX_CARDS; i++ )
	{
		gameinfo2_t *gi = EngFuncs::GetModInfo( i );
		if( !gi ) break;

		bool known = false;
		for( size_t k = 0; k < V_ARRAYSIZE( g_KnownGames ); k++ )
			if( !stricmp( gi->gamefolder, g_KnownGames[k].folder ))
				known = true;
		if( known )
			continue;

		card_t &c = m_Cards[m_iCount++];
		Q_strncpy( c.folder, gi->gamefolder, sizeof( c.folder ));
		Q_strncpy( c.title, gi->title, sizeof( c.title ));
		Q_strncpy( c.meta, gi->gamefolder, sizeof( c.meta ));
		GameArt( c.folder, c.art );
		GameBackdrop( c.folder, c.backdrop );
		c.current = !stricmp( currentGame, c.folder );
	}

	// land on the current game
	for( int i = 0; i < m_iCount; i++ )
		if( m_Cards[i].current )
			SelectCard( i, false );
}

void CMenuContGamePicker::SelectCard( int i, bool playSound )
{
	if( i < 0 || i >= m_iCount || ( i == m_iSel && playSound ))
		return;

	m_iSel = i;
	if( playSound )
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
}

void CMenuContGamePicker::ActivateCard()
{
	if( m_iSel < 0 || m_iSel >= m_iCount )
		return;

	card_t &c = m_Cards[m_iSel];

	if( c.current )
	{
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
		UI_ContGamePage_Menu();
		return;
	}

	static char cmd[96], msg[160];
	snprintf( cmd, sizeof( cmd ), "game %s\n", c.folder );
	snprintf( msg, sizeof( msg ), "Switch to %s? The engine will restart.", c.title );

	dialog.SetMessage( msg );
	dialog.onPositive.SetCommand( false, cmd );
	dialog.Show();
}

// logical-space rect of card i at current scroll
void CMenuContGamePicker::CardRect( int i, int &x, int &y, int &w, int &h )
{
	x = MARGIN + i * ( CARD_W + CARD_GAP ) - (int)m_flScroll;
	y = CARD_Y;
	w = CARD_W;
	h = CARD_H;
}

bool CMenuContGamePicker::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ))
	{
		Hide();
		return true;
	}

	if( UI::Key::IsLeftArrow( key ))
	{
		SelectCard( m_iSel - 1 );
		return true;
	}

	if( UI::Key::IsRightArrow( key ))
	{
		SelectCard( m_iSel + 1 );
		return true;
	}

	if( UI::Key::IsEnter( key ))
	{
		ActivateCard();
		return true;
	}

	if( key == K_MOUSE1 )
	{
		for( int i = 0; i < m_iCount; i++ )
		{
			int x, y, w, h;
			CardRect( i, x, y, w, h );
			UI_ScaleCoords( x, y, w, h );
			if( UI_CursorInRect( x, y, w, h ))
			{
				if( i == m_iSel )
					ActivateCard();
				else
					SelectCard( i );
				return true;
			}
		}
	}

	return CMenuFramework::KeyDown( key );
}

bool CMenuContGamePicker::MouseMove( int x, int y )
{
	for( int i = 0; i < m_iCount; i++ )
	{
		int cx, cy, cw, ch;
		CardRect( i, cx, cy, cw, ch );
		UI_ScaleCoords( cx, cy, cw, ch );
		if( UI_CursorInRect( cx, cy, cw, ch ) && i != m_iSel )
			SelectCard( i );
	}
	return true;
}

void CMenuContGamePicker::Draw()
{
	// scroll target: keep the focused card comfortably on screen; center the
	// row when it overflows the logical width
	const int rowW = m_iCount * CARD_W + ( m_iCount - 1 ) * CARD_GAP;
	float target = 0;

	if( rowW > uiStatic.width - 2 * MARGIN )
	{
		target = m_iSel * ( CARD_W + CARD_GAP ) - ( uiStatic.width - 2 * MARGIN - CARD_W ) / 2;
		target = bound( 0.0f, target, (float)( rowW - ( uiStatic.width - 2 * MARGIN )));
	}

	// exponential smoothing toward target
	const float dt = gpGlobals->frametime;
	m_flScroll += ( target - m_flScroll ) * bound( 0.0f, dt * 12.0f, 1.0f );

	// backdrop follows focused card
	if( m_iSel >= 0 && m_iSel < m_iCount && m_Cards[m_iSel].backdrop.IsValid( ))
		DrawBackdrop( m_Cards[m_iSel].backdrop );
	else
	{
		CImage none;
		DrawBackdrop( none );
	}

	// title
	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 70 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;
	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"GAME", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		"CHOOSE A CAMPAIGN", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// cards
	for( int i = 0; i < m_iCount; i++ )
	{
		card_t &c = m_Cards[i];
		const bool sel = ( i == m_iSel );

		int x, y, w, h;
		CardRect( i, x, y, w, h );

		// focused card scales up around its center
		if( sel )
		{
			const int gw = w * 0.06f, gh = h * 0.06f;
			x -= gw; y -= gh; w += gw * 2; h += gh * 2;
		}

		int artH = h - CARD_LBL_H * ( sel ? 1.06f : 1.0f );

		UI_ScaleCoords( x, y, w, h );
		int sArtH = artH * uiStatic.scaleY;
		int sLblH = h - sArtH;

		const unsigned int dim = sel ? 0xFFFFFFFF : 0xFF8E8E8E;

		// art (4:3 source onto 4:3 box; fit handles odd sizes)
		UI_FillRect( x, y, w, sArtH, 0xFF000000 );
		if( c.art.IsValid( ))
			DrawPicAspectFit( x, y, w, sArtH, c.art, dim );

		// label block
		UI_FillRect( x, y + sArtH, w, sLblH, sel ? 0xE614171D : 0xB414171D );

		const int nameH = 14 * uiStatic.scaleY;
		const int metaH = 11 * uiStatic.scaleY;
		const int px = x + 12 * uiStatic.scaleX;
		UI_DrawString( fontSmall, px, y + sArtH + 9 * uiStatic.scaleY, w - 16 * uiStatic.scaleX, nameH * 1.45f,
			c.title, sel ? clrInk : clrInkDim, nameH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );
		UI_DrawString( fontHint, px, y + sArtH + 9 * uiStatic.scaleY + nameH + 5 * uiStatic.scaleY,
			w - 16 * uiStatic.scaleX, metaH * 1.45f,
			c.meta, clrInkFaint, metaH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );

		// frame
		if( sel )
			UI_DrawRectangleExt( x, y, w, h, clrAccent, 2 );
		else
			UI_DrawRectangleExt( x, y, w, h, 0x23FFFFFF, 1 );

		// current-game tag
		if( c.current )
		{
			const int tagH = 10 * uiStatic.scaleY;
			const char *tag = "CURRENT";
			const int tagW = g_FontMgr->GetTextWideScaled( fontSmall, tag, tagH ) + 12 * uiStatic.scaleX;
			UI_FillRect( x + 8 * uiStatic.scaleX, y + 8 * uiStatic.scaleY, tagW, tagH + 8 * uiStatic.scaleY, clrAccent );
			UI_DrawString( fontSmall, x + 14 * uiStatic.scaleX, y + 12 * uiStatic.scaleY, tagW, tagH * 1.45f,
				tag, 0xFF14110A, tagH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
		}
	}

	CMenuFramework::Draw();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Select" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ), "SWITCHING GAME RESTARTS THE ENGINE" );
}

ADD_MENU( menu_continuum_games, CMenuContGamePicker, UI_ContGamePicker_Menu );
