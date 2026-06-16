/*
Character.cpp -- Continuum character setup: name, model, colors, live preview
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
#include "PlayerModelView.h"
#include "StringVectorModel.h"

using namespace Cont;

#define ROW_W       640
#define PANEL_W     560

class CMenuContCharacter;

// player model spinner: cycles models/player/*/<name>.mdl
class CContModelRow : public CContButton
{
public:
	CContModelRow() : iIndex( 0 ) { }

	void Refresh()
	{
		char **filenames;
		int numFiles;

		m_Models.RemoveAll();
		filenames = EngFuncs::GetFilesList( "models/player/*", &numFiles, false );

		for( int i = 0; i < numFiles; i++ )
		{
			char name[64], path[256];
			COM_FileBase( filenames[i], name, sizeof( name ));

			snprintf( path, sizeof( path ), "models/player/%s/%s.mdl", name, name );
			if( !EngFuncs::FileExists( path ))
				continue;

			m_Models.AddToTail( name );
		}

		// land on the current model
		const char *current = EngFuncs::GetCvarString( "model" );
		iIndex = -1;
		FOR_EACH_VEC( m_Models, i )
		{
			if( !stricmp( m_Models[i].String(), current ))
			{
				iIndex = i;
				break;
			}
		}

		// model not installed locally: show the cvar value read-only-ish
		if( iIndex < 0 && m_Models.Count( ))
			iIndex = 0;

		Sync();
	}

	const char *Current() const
	{
		return m_Models.IsValidIndex( iIndex ) ? m_Models[iIndex].String() : NULL;
	}

	bool KeyDown( int key ) override
	{
		int dir = 0;

		if( UI::Key::IsLeftArrow( key ))
			dir = -1;
		else if( UI::Key::IsRightArrow( key ))
			dir = 1;
		else if( key == K_MOUSE1 && UI_CursorInRect( m_scPos, m_scSize ))
			dir = uiStatic.cursorX > m_scPos.x + m_scSize.w * 0.78f ? 1 : -1;

		if( dir )
		{
			if( m_Models.Count( ))
			{
				iIndex = ( iIndex + dir + m_Models.Count( )) % m_Models.Count();
				Sync();
				PlayLocalSound( uiStatic.sounds[SND_MOVE] );
				_Event( QM_CHANGED );
			}
			return true;
		}
		return CContButton::KeyDown( key );
	}

	int iIndex;

private:
	void Sync()
	{
		const char *cur = Current();
		szValue = cur ? cur : EngFuncs::GetCvarString( "model" );
	}

	CUtlVector<CUtlString> m_Models;
};

class CMenuContCharacter : public CMenuFramework
{
public:
	CMenuContCharacter() : CMenuFramework( "CMenuContCharacter" ) { }

	bool KeyDown( int key ) override;
	void Draw() override;
	void Show() override;
	void Hide() override;

private:
	void _Init() override;
	void _VidInit() override;

	void ApplyModel();
	void ApplyColors();

	CContTextRow playerName;
	CContModelRow model;
	CContSliderRow topColor;
	CContSliderRow bottomColor;
	CContToggleRow show3D;

	CMenuPlayerModelView view;
};

void CMenuContCharacter::ApplyModel()
{
	const char *mdl = model.Current();
	if( !mdl )
		return;

	EngFuncs::CvarSetString( "model", mdl );

	char path[256];
	snprintf( path, sizeof( path ), "models/player/%s/%s.bmp", mdl, mdl );
	view.hPlayerImage = EngFuncs::PIC_Load( path, PIC_KEEP_SOURCE );
	ApplyColors();

	if( !strcmp( mdl, "player" ))
		Q_strncpy( path, "models/player.mdl", sizeof( path ));
	else
		snprintf( path, sizeof( path ), "models/player/%s/%s.mdl", mdl, mdl );

	if( view.ent )
		EngFuncs::SetModel( view.ent, path );
}

void CMenuContCharacter::ApplyColors()
{
	EngFuncs::ProcessImage( view.hPlayerImage, -1,
		(int)EngFuncs::GetCvarFloat( "topcolor" ), (int)EngFuncs::GetCvarFloat( "bottomcolor" ));
}

void CMenuContCharacter::_Init()
{
	playerName.SetNameAndStatus( "Player Name", NULL );
	playerName.szHint = "Shown to other players";
	playerName.Setup( "name", 31 );

	model.SetNameAndStatus( "Player Model", NULL );
	model.szHint = "How you look to everyone else";
	model.onChanged = VoidCb( &CMenuContCharacter::ApplyModel );

	topColor.SetNameAndStatus( "Shirt Color", NULL );
	topColor.Setup( "topcolor", 0, 255, 5, 30, 0 );
	topColor.onChanged = VoidCb( &CMenuContCharacter::ApplyColors );

	bottomColor.SetNameAndStatus( "Pants Color", NULL );
	bottomColor.Setup( "bottomcolor", 0, 255, 5, 6, 0 );
	bottomColor.onChanged = VoidCb( &CMenuContCharacter::ApplyColors );

	show3D.SetNameAndStatus( "3D Preview", NULL );
	show3D.szHint = "Render the model instead of its portrait";
	show3D.Setup( "ui_showmodels", 1 );

	AddItem( playerName );
	AddItem( model );
	AddItem( topColor );
	AddItem( bottomColor );
	AddItem( show3D );
	AddItem( view );
}

void CMenuContCharacter::_VidInit()
{
	VidInitFonts();

	const int itemH = 56, gap = 6;
	int y = 280;

	playerName.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	model.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	topColor.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	bottomColor.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	show3D.SetRect( MARGIN, y, ROW_W, itemH );

	// the preview pane takes whatever fits right of the rows — never under them
	const int panelX = Q_max( MARGIN + ROW_W + 32, (int)uiStatic.width - MARGIN - PANEL_W );
	const int panelW = uiStatic.width - MARGIN - panelX;
	view.SetRect( panelX, 200, panelW, 420 );

	// theme the model viewport
	view.backgroundColor = 0xC0101218u;
	view.colorStroke = 0x23FFFFFFu;
}

void CMenuContCharacter::Show()
{
	CMenuFramework::Show();
	playerName.Reload();
	model.Refresh();
	ApplyModel();
}

void CMenuContCharacter::Hide()
{
	EngFuncs::ClientCmd( false, "host_writeconfig\n" );
	CMenuFramework::Hide();
}

bool CMenuContCharacter::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ))
	{
		if( playerName.IsEditing( ))
			return playerName.KeyDown( key );
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

void CMenuContCharacter::Draw()
{
	static CImage noBackdrop;
	// panel behind the rows column only; the model preview on the right keeps its own bg
	DrawScreenBackdrop( noBackdrop, MARGIN - 30, ROW_W + 40 );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"CHARACTER", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		"SHARED BY ALL GAMES", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	CMenuFramework::Draw();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Change" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ), "DRAG THE MODEL TO ROTATE" );
}

ADD_MENU( menu_continuum_character, CMenuContCharacter, UI_ContCharacter_Menu );
