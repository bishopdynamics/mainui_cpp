/*
GamePage.cpp -- Continuum per-game page: art + New/Load/Save/Multiplayer
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
#include "FontManager.h"
#include "YesNoMessageBox.h"
#include "keydefs.h"

using namespace Cont;

static const char *g_szSkillNames[] = { "Easy", "Normal", "Hard" };

// "New Game" row: left/right cycles difficulty, A starts
class CContNewGameRow : public CContButton
{
public:
	typedef CContButton BaseClass;
	CContNewGameRow() : BaseClass(), iSkill( 1 ) { }

	bool KeyDown( int key ) override
	{
		if( UI::Key::IsLeftArrow( key ) || UI::Key::IsRightArrow( key ))
		{
			const int dir = UI::Key::IsRightArrow( key ) ? 1 : -1;
			iSkill = bound( 0, iSkill + dir, 2 );
			szValue = g_szSkillNames[iSkill];
			PlayLocalSound( uiStatic.sounds[SND_MOVE] );
			return true;
		}
		return BaseClass::KeyDown( key );
	}

	int iSkill; // 0..2 -> skill 1..3
};

class CMenuContGamePage : public CMenuFramework
{
public:
	CMenuContGamePage() : CMenuFramework( "CMenuContGamePage" ) { }

	bool KeyDown( int key ) override;
	void Draw() override;
	void Show() override;

private:
	void _Init() override;
	void _VidInit() override;

	void StartNewGame();
	void NewGameCb();

	CContNewGameRow newGame;
	CContButton loadGame;
	CContButton saveGame;
	CContButton multiplayer;

	CMenuYesNoMessageBox dialog;

	CImage art;
	CImage backdrop;
	char szMeta[128];
};

void CMenuContGamePage::StartNewGame()
{
	EngFuncs::CvarSetValue( "skill", newGame.iSkill + 1.0f );
	EngFuncs::CvarSetValue( "deathmatch", 0.0f );
	EngFuncs::CvarSetValue( "teamplay", 0.0f );
	EngFuncs::CvarSetValue( "pausable", 1.0f );
	EngFuncs::CvarSetValue( "coop", 0.0f );
	EngFuncs::CvarSetValue( "maxplayers", 1.0f );

	EngFuncs::PlayBackgroundTrack( NULL, NULL );
	EngFuncs::ClientCmd( false, "newgame\n" );
}

void CMenuContGamePage::NewGameCb()
{
	if( CL_IsActive() && gpGlobals->maxClients < 2 )
	{
		// don't silently throw away a running singleplayer game
		dialog.SetMessage( L( "StringsList_240" )); // "Starting a new game will exit any current game..."
		dialog.onPositive = VoidCb( &CMenuContGamePage::StartNewGame );
		dialog.Show();
	}
	else
		StartNewGame();
}

bool CMenuContGamePage::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ))
	{
		Hide();
		return true;
	}

	if( key == K_MOUSE1 )
	{
		const int legendKey = Cont::LegendClickKey();
		if( legendKey )
			return KeyDown( legendKey );
	}

	return CMenuFramework::KeyDown( key );
}

void CMenuContGamePage::_Init()
{
	newGame.SetNameAndStatus( "New Game", NULL );
	newGame.szHint = "Left/right sets difficulty";
	newGame.szValue = g_szSkillNames[newGame.iSkill];
	newGame.onReleased = VoidCb( &CMenuContGamePage::NewGameCb );

	loadGame.SetNameAndStatus( "Load Game", NULL );
	loadGame.szHint = "Pick up where you left off";
	loadGame.onReleased = UI_LoadGame_Menu;

	saveGame.SetNameAndStatus( "Save Game", NULL );
	saveGame.szHint = "Available while playing";
	saveGame.onReleased = UI_SaveGame_Menu;

	multiplayer.SetNameAndStatus( "Multiplayer", NULL );
	multiplayer.szHint = "Browse servers or host your own";
	multiplayer.onReleased = UI_MultiPlayer_Menu;

	dialog.Link( this );

	AddItem( newGame );
	AddItem( loadGame );
	AddItem( saveGame );
	AddItem( multiplayer );
}

void CMenuContGamePage::Show()
{
	CMenuFramework::Show();

	// contextual availability
	const bool inSingle = CL_IsActive() && gpGlobals->maxClients < 2;
	saveGame.SetGrayed( !inSingle );
	saveGame.szHint = inSingle ? "Save your progress" : "Available while playing";

	if( gMenu.m_gameinfo.gamemode == GAME_MULTIPLAYER_ONLY || gMenu.m_gameinfo.startmap[0] == 0 )
		newGame.SetGrayed( true );

	multiplayer.SetVisibility( gMenu.m_gameinfo.gamemode != GAME_SINGLEPLAYER_ONLY );

	if( !EngFuncs::CheckGameDll( ))
	{
		newGame.SetGrayed( true );
		saveGame.SetGrayed( true );
	}
}

void CMenuContGamePage::_VidInit()
{
	VidInitFonts();

	const char *folder = gMenu.m_gameinfo.gamefolder;
	GameArt( folder, art );
	GameBackdrop( folder, backdrop );

	snprintf( szMeta, sizeof( szMeta ), "%s - STREAMING", folder );
	for( char *p = szMeta; *p; p++ )
		*p = toupper( *p );

	const int itemH = 56, gap = 6;
	int y = 768 - LEGEND_H - 40 - 4 * ( itemH + gap );

	newGame.SetRect( MARGIN, y, 430, itemH );
	y += itemH + gap;
	loadGame.SetRect( MARGIN, y, 430, itemH );
	y += itemH + gap;
	saveGame.SetRect( MARGIN, y, 430, itemH );
	y += itemH + gap;
	multiplayer.SetRect( MARGIN, y, 430, itemH );
}

void CMenuContGamePage::Draw()
{
	DrawBackdrop( backdrop );

	// title block
	const int tx = MARGIN * uiStatic.scaleX;
	int ty = 96 * uiStatic.scaleY;
	const int titleH = 34 * uiStatic.scaleY;


	UI_DrawString( fontBrand, tx, ty, ScreenWidth * 0.45f, titleH * 2.9f,
		gMenu.m_gameinfo.title, clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// the game's own art, aspect-fit in a right-side frame
	if( art.IsValid( ))
	{
		const int frameX = ( uiStatic.width - MARGIN - 560 ) * uiStatic.scaleX;
		const int frameY = 120 * uiStatic.scaleY;
		const int frameW = 560 * uiStatic.scaleX;
		const int frameH = 420 * uiStatic.scaleY;

		UI_FillRect( frameX - 2, frameY - 2, frameW + 4, frameH + 4, 0x96000000 );
		DrawPicAspectFit( frameX, frameY, frameW, frameH, art );
		UI_DrawRectangleExt( frameX, frameY, frameW, frameH, 0x23FFFFFF, 1 );
	}

	CMenuFramework::Draw();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Select" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ), "XASH3D - CONTINUUM BUILD" );
}

ADD_MENU( menu_continuum_gamepage, CMenuContGamePage, UI_ContGamePage_Menu );
