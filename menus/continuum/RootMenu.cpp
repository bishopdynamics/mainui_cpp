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
	dialog.SetMessage( L( "GameUI_QuitConfirmationText" ));
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
	AddItem( leaveGame );
	AddItem( game );
	AddItem( configuration );
	AddItem( quit );
}

void CMenuContRoot::LayoutRows()
{
	const bool connected = CL_IsActive();
	m_bLastConnected = connected;

	resumeGame.SetVisibility( connected );
	leaveGame.SetVisibility( connected );
	leaveGame.szName = gpGlobals->maxClients > 1 ? "Disconnect" : "Main Menu";

	int y = 300;
	const int itemH = 64, gap = 6;

	if( connected )
	{
		resumeGame.SetRect( MARGIN, y, 420, itemH );
		y += itemH + gap;
		leaveGame.SetRect( MARGIN, y, 420, itemH );
		y += itemH + gap;
	}
	game.SetRect( MARGIN, y, 420, itemH );
	y += itemH + gap;
	configuration.SetRect( MARGIN, y, 420, itemH );
	y += itemH + gap;
	quit.SetRect( MARGIN, y, 420, itemH );

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

	DrawBackdrop( backdrop );

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

	// returns the rightmost x reached
	x = UI_DrawString( fontBrand, x, by, ScreenWidth, brandH * 1.45f,
		"HALF-LIFE", clrInk, brandH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	const int subH = 13 * uiStatic.scaleY;
	UI_DrawString( fontSmall, x + 22 * uiStatic.scaleX, by + brandH - subH, ScreenWidth, subH * 1.45f,
		"C O N T I N U U M   E D I T I O N", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// current game context, top right (useful when booted into an expansion)
	const int ctxH = 19 * uiStatic.scaleY;
	const char *title = gMenu.m_gameinfo.title;
	int wide = g_FontMgr->GetTextWideScaled( fontItem, title, ctxH );
	UI_DrawString( fontItem, ScreenWidth - MARGIN * uiStatic.scaleX - wide, by + 4 * uiStatic.scaleY,
		wide + 4, ctxH * 1.45f, title, clrInkDim, ctxH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	CMenuFramework::Draw();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Select" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ), "XASH3D - CONTINUUM BUILD" );
}

ADD_MENU( menu_main, CMenuContRoot, UI_Main_Menu );
