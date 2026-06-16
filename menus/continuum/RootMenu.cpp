/*
RootMenu.cpp -- Continuum root menu: Game / Configuration / Quit
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

class CMenuContRoot : public CMenuFramework
{
public:
	CMenuContRoot() : CMenuFramework( "CMenuContRoot" ) { }

	bool KeyDown( int key ) override;
	void Draw() override;
	void Show() override;

private:
	void _Init() override;
	void _VidInit() override;

	void QuitDialogCb();
	void LeaveGameCb();
	void LayoutRows();

	CContButton resumeGame;
	CContButton saveGame;
	CContButton loadGame;
	CContButton cheats;
	CContButton leaveGame;
	CContButton game;
	CContButton configuration;
	CContButton quit;

	CMenuYesNoMessageBox dialog;

	CImage backdrop;
	CImage lambda;
	bool m_bLastConnected = false;
};

void CMenuContRoot::QuitDialogCb()
{
	dialog.SetMessage( "Are you sure you want to quit the game?" );
	dialog.onPositive.SetCommand( false, "quit\n" );
	dialog.Show();
}

void CMenuContRoot::LeaveGameCb()
{
	if( gpGlobals->maxClients > 1 )
		dialog.SetMessage( "Disconnect from this server?" );
	else
		dialog.SetMessage( "Leave the current game? Progress since your last save will be lost." );

	dialog.onPositive.SetCommand( false, "disconnect\n" );
	dialog.Show();
}

bool CMenuContRoot::KeyDown( int key )
{
	// the controller's menu/start button resumes a paused game, like Esc
	if( key == K_START_BUTTON && CL_IsActive( ))
	{
		if( !dialog.IsVisible( ))
			UI_CloseMenu();
		return true;
	}

	if( UI::Key::IsEscape( key ))
	{
		if( CL_IsActive( ))
		{
			if( !dialog.IsVisible( ))
				UI_CloseMenu();
		}
		else
			QuitDialogCb();
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

void CMenuContRoot::Show()
{
	CMenuFramework::Show();

	// after an in-menu game switch the engine restarts; take the user
	// straight to the page of the game they just picked
	static bool checkedBreadcrumb = false;
	if( !checkedBreadcrumb )
	{
		checkedBreadcrumb = true;
		if( EngFuncs::GetCvarFloat( "host_changegame_boot" ) != 0.0f )
		{
			EngFuncs::CvarSetValue( "host_changegame_boot", 0.0f );
			UI_ContGamePage_Menu();
		}
	}

	// the cursor starts on item 0 (Resume), which is hidden outside a game;
	// land focus on the first visible item instead
	CMenuBaseItem *cur = ItemAtCursor();
	if( !cur || !cur->IsVisible( ))
	{
		for( int i = 0; i < m_pItems.Count(); i++ )
		{
			if( m_pItems[i]->IsVisible() && !FBitSet( m_pItems[i]->iFlags, QMF_INACTIVE ))
			{
				SetCursor( i );
				break;
			}
		}
	}
}

void CMenuContRoot::_Init()
{
	resumeGame.SetNameAndStatus( L( "GameUI_GameMenu_ResumeGame" ), NULL );
	resumeGame.onReleased = UI_CloseMenu;

	saveGame.SetNameAndStatus( "Save Game", NULL );
	saveGame.onReleased = UI_ContSaveGame_Menu;

	loadGame.SetNameAndStatus( "Load Game", NULL );
	loadGame.onReleased = UI_ContLoadGame_Menu;

	cheats.SetNameAndStatus( "Cheats", NULL );
	cheats.onReleased = UI_ContCheats_Menu;

	leaveGame.SetNameAndStatus( "Main Menu", NULL );
	leaveGame.onReleased = VoidCb( &CMenuContRoot::LeaveGameCb );

	game.SetNameAndStatus( "Game", NULL );
	SET_EVENT( game.onReleased, UI_ContGamePicker_Menu( ));

	configuration.SetNameAndStatus( "Configuration", NULL );
	SET_EVENT( configuration.onReleased, UI_ContConfig_Menu( ));

	quit.SetNameAndStatus( "Quit", NULL );
	quit.onReleased = VoidCb( &CMenuContRoot::QuitDialogCb );

	dialog.Link( this );

	AddItem( resumeGame );
	AddItem( saveGame );
	AddItem( loadGame );
	AddItem( cheats );
	AddItem( leaveGame );
	AddItem( game );
	AddItem( configuration );
	AddItem( quit );
}

void CMenuContRoot::LayoutRows()
{
	const bool connected = CL_IsActive();
	const bool single = connected && gpGlobals->maxClients < 2;
	m_bLastConnected = connected;

	// mid-game the menu is about THIS game: save/load (singleplayer) and
	// leaving; switching games means going through Main Menu first
	const bool cheatsOn = EngFuncs::GetCvarFloat( "sv_cheats" ) != 0.0f;
	resumeGame.SetVisibility( connected );
	saveGame.SetVisibility( single );
	loadGame.SetVisibility( single );
	cheats.SetVisibility( single && cheatsOn );
	leaveGame.SetVisibility( connected );
	leaveGame.szName = gpGlobals->maxClients > 1 ? "Disconnect" : "Main Menu";
	game.SetVisibility( !connected );

	// collect the rows that are actually showing, in display order, so the
	// stack height tracks the row count (mid-game with cheats on is the tall
	// case: Resume/Save/Load/Cheats/Main Menu/Configuration/Quit)
	CContButton *rows[8];
	int n = 0;
	if( connected )
	{
		rows[n++] = &resumeGame;
		if( single )
		{
			rows[n++] = &saveGame;
			rows[n++] = &loadGame;
			if( cheatsOn )
				rows[n++] = &cheats;
		}
		rows[n++] = &leaveGame;
	}
	else
		rows[n++] = &game;
	rows[n++] = &configuration;
	rows[n++] = &quit;

	const int itemH = 64, gap = 6;
	const int blockH = n * itemH + ( n - 1 ) * gap;

	// preferred upper-third anchor, but never let the stack run under the bottom
	// legend/input-prompts bar: shift the whole block up just enough to fit
	const int bottomLimit = 768 - LEGEND_H - 24; // logical space is 768 tall
	int y = 300;
	if( y + blockH > bottomLimit )
		y = bottomLimit - blockH;

	for( int i = 0; i < n; i++ )
	{
		rows[i]->SetRect( MARGIN, y, 420, itemH );
		y += itemH + gap;
	}

	// SetRect only stores logical coords; rescale them now
	CalcItemsPositions();
	CalcItemsSizes();
}

void CMenuContRoot::_VidInit()
{
	VidInitFonts();
	lambda.Load( "gfx/shell/continuum/lambda.png" );

	GameBackdrop( gMenu.m_gameinfo.gamefolder, backdrop );

	LayoutRows();
}

void CMenuContRoot::Draw()
{
	// the in-game rows (Resume / Main Menu) come and go with the connection,
	// e.g. right after Main Menu disconnects while this screen stays up
	if( CL_IsActive() != m_bLastConnected )
	{
		LayoutRows();

		CMenuBaseItem *cur = ItemAtCursor();
		if( !cur || !cur->IsVisible( ))
		{
			for( int i = 0; i < m_pItems.Count(); i++ )
			{
				if( m_pItems[i]->IsVisible() && !FBitSet( m_pItems[i]->iFlags, QMF_INACTIVE ))
				{
					SetCursor( i );
					break;
				}
			}
		}
	}

	// the brand (logo + game name + subtitle) sits in a full-width panel across the top,
	// so a long game name has all the room it needs; the buttons get their own column
	// panel below it
	DrawSeethruBackdrop( backdrop );
	const int topH = 110;
	const int gap = 8;
	DrawContentPanel( MARGIN - 30, 44, (int)uiStatic.width - 2 * ( MARGIN - 30 ), topH );
	DrawContentPanel( MARGIN - 30, 44 + topH + gap, 480, ( 768 - LEGEND_H - 14 ) - ( 44 + topH + gap ));

	// brand line
	const int bx = MARGIN * uiStatic.scaleX;
	const int by = 70 * uiStatic.scaleY;
	const int brandH = 36 * uiStatic.scaleY;
	const int lambdaH = 42 * uiStatic.scaleY;
	int x = bx;

	if( lambda.IsValid( ))
	{
		const int lw = lambdaH * EngFuncs::PIC_Width( lambda.Handle( )) / EngFuncs::PIC_Height( lambda.Handle( ));
		UI_DrawPic( x, by - 2 * uiStatic.scaleY, lw, lambdaH, 0xFFFFFFFF, lambda );
		x += lw + 16 * uiStatic.scaleX;
	}

	// brand: the loaded game's name, with "Continuum Edition" stacked on the line below it
	// (replaces the fixed "HALF-LIFE", and the game name no longer needs the top-right slot)
	char gameName[64];
	Q_strncpy( gameName, gMenu.m_gameinfo.title, sizeof( gameName ));
	for( char *c = gameName; *c; c++ )
		if( *c >= 'a' && *c <= 'z' ) *c -= 'a' - 'A';

	UI_DrawString( fontBrand, x, by, ScreenWidth, brandH * 1.45f,
		gameName, clrInk, brandH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	const int subH = 13 * uiStatic.scaleY;
	UI_DrawString( fontSmall, x, by + brandH - 2 * uiStatic.scaleY + 2 * subH, ScreenWidth, subH * 1.45f,
		"C O N T I N U U M   E D I T I O N", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	CMenuFramework::Draw();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Select" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ), "XASH3D - CONTINUUM BUILD" );
}

// UI_Main_Menu picks the menu family: the Continuum root by default, the
// classic menu when ui_classic is set. The toggles rebuild the menu through
// here immediately (no restart), and when they do, land straight on the
// OTHER family's page with its menu-style toggle focused so the user can
// keep flipping back and forth with the activate button
bool g_bUiFamilySwitch = false;

ADD_MENU3( menu_main, CMenuContRoot, UI_Main_Menu );
void UI_Main_Menu( void )
{
	const bool classic = EngFuncs::GetCvarFloat( "ui_classic" ) != 0.0f;

	if( classic )
		UI_MainClassic_Menu();
	else
		menu_main->Show();

	if( g_bUiFamilySwitch )
	{
		g_bUiFamilySwitch = false;

		if( classic )
			UI_AdvSettings2_FocusUiToggle();
		else
			UI_ContConfig_FocusUiToggle();
	}
}
