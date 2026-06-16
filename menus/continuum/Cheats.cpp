/*
Cheats.cpp -- Continuum cheats screen: god/noclip/etc. toggles + give buttons
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

using namespace Cont;

#define COL_W   420
#define ROW_H   56
#define GAP     6

// The toggle cheats (god/notarget/noclip) are game-DLL toggle commands with no
// queryable state, so we track desired state in the engine's cheat_* cvars: the
// row flips the cvar, onChanged forwards the command to match, and the engine
// re-applies the cvars after each seamless level change (SV_ChangeLevel).
class CMenuContCheats : public CMenuFramework
{
public:
	CMenuContCheats() : CMenuFramework( "CMenuContCheats" ) { }

	bool KeyDown( int key ) override;
	void Draw() override;
	void Show() override;

private:
	void _Init() override;
	void _VidInit() override;

	CContHeader     hdrToggles, hdrGive;
	CContToggleRow  god, notarget, noclip, thirdPerson;
	CContButton     giveAll, silent, aiPaths;
	CContButton     giveSuit, giveBattery, giveMedkit, giveLongjump;
};

void CMenuContCheats::_Init()
{
	hdrToggles.SetNameAndStatus( "TOGGLES", NULL );
	hdrGive.SetNameAndStatus( "GIVE & DO", NULL );

	god.SetNameAndStatus( "God Mode", NULL );
	god.szHint = "Take no damage";
	god.Setup( "cheat_god", 0 );
	SET_EVENT_MULTI( god.onChanged,
	{
		(void)pSelf; (void)pExtra;
		EngFuncs::ClientCmd( false, "god\n" );
	});

	notarget.SetNameAndStatus( "No Target", NULL );
	notarget.szHint = "Enemies ignore you";
	notarget.Setup( "cheat_notarget", 0 );
	SET_EVENT_MULTI( notarget.onChanged,
	{
		(void)pSelf; (void)pExtra;
		EngFuncs::ClientCmd( false, "notarget\n" );
	});

	noclip.SetNameAndStatus( "Noclip", NULL );
	noclip.szHint = "Fly through walls";
	noclip.Setup( "cheat_noclip", 0 );
	SET_EVENT_MULTI( noclip.onChanged,
	{
		(void)pSelf; (void)pExtra;
		EngFuncs::ClientCmd( false, "noclip\n" );
	});

	thirdPerson.SetNameAndStatus( "Third Person", NULL );
	thirdPerson.szHint = "Chase camera behind the player";
	thirdPerson.Setup( "cheat_thirdperson", 0 );
	SET_EVENT_MULTI( thirdPerson.onChanged,
	{
		(void)pSelf; (void)pExtra;
		// thirdperson/firstperson are setters, not a toggle - pick by state
		EngFuncs::ClientCmd( false,
			EngFuncs::GetCvarFloat( "cheat_thirdperson" ) != 0.0f ? "thirdperson\n" : "firstperson\n" );
	});

	giveAll.SetNameAndStatus( "Give All Weapons", NULL );
	giveAll.onReleased.SetCommand( false, "impulse 101\n" );

	giveSuit.SetNameAndStatus( "Give HEV Suit", NULL );
	giveSuit.onReleased.SetCommand( false, "give item_suit\n" );

	giveBattery.SetNameAndStatus( "Give Armor", NULL );
	giveBattery.onReleased.SetCommand( false, "give item_battery\n" );

	giveMedkit.SetNameAndStatus( "Give Medkit", NULL );
	giveMedkit.onReleased.SetCommand( false, "give item_healthkit\n" );

	giveLongjump.SetNameAndStatus( "Give Longjump", NULL );
	giveLongjump.onReleased.SetCommand( false, "give item_longjump\n" );

	silent.SetNameAndStatus( "Silent to Monsters", NULL );
	silent.onReleased.SetCommand( false, "impulse 105\n" );

	aiPaths.SetNameAndStatus( "Show AI Paths", NULL );
	aiPaths.onReleased.SetCommand( false, "impulse 195\n" );

	AddItem( hdrToggles );
	AddItem( god );
	AddItem( notarget );
	AddItem( noclip );
	AddItem( thirdPerson );

	AddItem( hdrGive );
	AddItem( giveAll );
	AddItem( giveSuit );
	AddItem( giveBattery );
	AddItem( giveMedkit );
	AddItem( giveLongjump );
	AddItem( silent );
	AddItem( aiPaths );
}

void CMenuContCheats::_VidInit()
{
	VidInitFonts();

	const int colL = MARGIN;
	const int colR = MARGIN + COL_W + 40;
	int y;

	// left column: toggle cheats
	y = 280;
	hdrToggles.SetRect( colL, y, COL_W, 36 ); y += 36 + GAP;
	god.SetRect( colL, y, COL_W, ROW_H ); y += ROW_H + GAP;
	notarget.SetRect( colL, y, COL_W, ROW_H ); y += ROW_H + GAP;
	noclip.SetRect( colL, y, COL_W, ROW_H ); y += ROW_H + GAP;
	thirdPerson.SetRect( colL, y, COL_W, ROW_H );

	// right column: one-shot give / action buttons
	y = 280;
	hdrGive.SetRect( colR, y, COL_W, 36 ); y += 36 + GAP;
	giveAll.SetRect( colR, y, COL_W, ROW_H ); y += ROW_H + GAP;
	giveSuit.SetRect( colR, y, COL_W, ROW_H ); y += ROW_H + GAP;
	giveBattery.SetRect( colR, y, COL_W, ROW_H ); y += ROW_H + GAP;
	giveMedkit.SetRect( colR, y, COL_W, ROW_H ); y += ROW_H + GAP;
	giveLongjump.SetRect( colR, y, COL_W, ROW_H ); y += ROW_H + GAP;
	silent.SetRect( colR, y, COL_W, ROW_H ); y += ROW_H + GAP;
	aiPaths.SetRect( colR, y, COL_W, ROW_H );
}

void CMenuContCheats::Show()
{
	CMenuFramework::Show();
	// reflect current desired state on each open
	god.Reload();
	notarget.Reload();
	noclip.Reload();
	thirdPerson.Reload();
}

bool CMenuContCheats::KeyDown( int key )
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

void CMenuContCheats::Draw()
{
	static CImage noBackdrop;
	// panel spans both cheat columns
	DrawScreenBackdrop( noBackdrop, MARGIN - 30, 2 * COL_W + 40 + 50 );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"CHEATS", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		"TOGGLES STAY ON THROUGH LEVEL CHANGES", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	CMenuFramework::Draw();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Select" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ), "CHEATS ARE ENABLED" );
}

ADD_MENU( menu_continuum_cheats, CMenuContCheats, UI_ContCheats_Menu );
