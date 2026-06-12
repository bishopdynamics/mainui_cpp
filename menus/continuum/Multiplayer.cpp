/*
Multiplayer.cpp -- Continuum multiplayer: hub, server browser, host game
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

#define ROW_W       760
#define ROW_H       52
#define CONTENT_TOP 208
#define CONTENT_BOTTOM ( 768 - LEGEND_H - 22 )

void UI_ContServers_Menu( void );
void UI_ContHostGame_Menu( void );


/*
====================
multiplayer hub
====================
*/
class CMenuContMultiplayer : public CMenuFramework
{
public:
	CMenuContMultiplayer() : CMenuFramework( "CMenuContMultiplayer" ) { }

	bool KeyDown( int key ) override;
	void Draw() override;
	void Show() override;

private:
	void _Init() override;
	void _VidInit() override;

	CContTextRow playerName;
	CContButton findServers;
	CContButton hostGame;
	CContButton character;

	CImage backdrop;
};

void CMenuContMultiplayer::_Init()
{
	playerName.SetNameAndStatus( "Player Name", NULL );
	playerName.szHint = "Shown to other players";
	playerName.Setup( "name", 31 );

	findServers.SetNameAndStatus( "Find Servers", NULL );
	findServers.szHint = "Browse internet and LAN games";
	SET_EVENT( findServers.onReleased, UI_ContServers_Menu( ));

	hostGame.SetNameAndStatus( "Host Game", NULL );
	hostGame.szHint = "Start a listen server others can join";
	SET_EVENT( hostGame.onReleased, UI_ContHostGame_Menu( ));

	character.SetNameAndStatus( "Character Setup", NULL );
	character.szHint = "Model and colors";
	character.onReleased = UI_ContCharacter_Menu;

	AddItem( playerName );
	AddItem( findServers );
	AddItem( hostGame );
	AddItem( character );
}

void CMenuContMultiplayer::_VidInit()
{
	VidInitFonts();
	GameBackdrop( gMenu.m_gameinfo.gamefolder, backdrop );

	const int itemH = 56, gap = 6;
	int y = 280;

	playerName.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	findServers.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	hostGame.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	character.SetRect( MARGIN, y, ROW_W, itemH );
}

void CMenuContMultiplayer::Show()
{
	CMenuFramework::Show();

	// historical engine quirk: clear stale prediction-off setting once
	if( EngFuncs::GetCvarFloat( "menu_mp_firsttime2" ) && EngFuncs::GetCvarFloat( "cl_nopred" ))
		EngFuncs::CvarSetValue( "cl_nopred", 0.0f );

	playerName.Reload();
}

bool CMenuContMultiplayer::KeyDown( int key )
{
	// the base window hides on Esc before items ever see it, so hand the key
	// to an editing row ourselves (it cancels the edit and nothing else)
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

void CMenuContMultiplayer::Draw()
{
	DrawBackdrop( backdrop );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	char sub[96];
	snprintf( sub, sizeof( sub ), "%s", gMenu.m_gameinfo.title );
	for( char *p = sub; *p; p++ )
		*p = toupper( *p );

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"MULTIPLAYER", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		sub, clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	CMenuFramework::Draw();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Select" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ));
}

ADD_MENU( menu_continuum_multiplayer, CMenuContMultiplayer, UI_ContMultiplayer_Menu );

/*
====================
server browser: one flat list, INTERNET/LAN sources, ping-sorted
====================
*/
class CMenuContServers : public CMenuFramework
{
public:
	CMenuContServers() : CMenuFramework( "CMenuContServers" ),
		m_bLan( false ), m_iSel( 0 ), m_flScroll( 0 ), m_flScrollTarget( 0 ),
		m_flRefreshTime( 0 ), m_flQueryTime( 0 ), m_bAskPassword( false ) { }

	bool KeyDown( int key ) override;
	bool KeyUp( int key ) override;
	bool MouseMove( int x, int y ) override;
	void Char( int ch ) override;
	void Draw() override;
	void Show() override;

	// feed from the engine's server-info callback; true when consumed
	bool AddServer( netadr_t adr, const char *info );

private:
	void _VidInit() override;

	struct srv_t
	{
		netadr_t adr;
		char name[64];
		char map[32];
		char players[16];
		char pingstr[16];
		int numcl, maxcl;
		float ping;
		bool password;
		bool isGoldSrc;
	};

	void Refresh();
	void SetSource( bool lan );
	void JoinSelected();
	void Connect( srv_t &s );
	void EnsureVisible();
	int RowAtCursor();
	int ListW() const { return uiStatic.width - 2 * MARGIN; }

	CUtlVector<srv_t> m_Servers;
	bool m_bLan;
	int m_iSel;
	float m_flScroll, m_flScrollTarget;
	float m_flRefreshTime;   // DoubleTime base for ping math
	float m_flQueryTime;     // uiStatic.realTime of last query, for "searching..."
	bool m_bMousePressed = false;

	// password entry overlay
	bool m_bAskPassword;
	char m_szPassword[32];
	srv_t m_PendingJoin;
};

void CMenuContServers::_VidInit()
{
	VidInitFonts();
}

void CMenuContServers::Show()
{
	CMenuFramework::Show();
	m_bAskPassword = false;
	Refresh();
}

void CMenuContServers::SetSource( bool lan )
{
	if( m_bLan == lan )
		return;

	m_bLan = lan;
	EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
	Refresh();
}

void CMenuContServers::Refresh()
{
	m_Servers.RemoveAll();
	m_iSel = 0;
	m_flScroll = m_flScrollTarget = 0;
	m_flRefreshTime = EngFuncs::DoubleTime();
	m_flQueryTime = uiStatic.realTime;

	if( m_bLan )
		EngFuncs::ClientCmd( false, "localservers\n" );
	else
		EngFuncs::ClientCmd( false, "internetservers\n" );
}

bool CMenuContServers::AddServer( netadr_t adr, const char *info )
{
	if( !IsVisible( ))
		return false;

	// only this game's servers
	if( stricmp( gMenu.m_gameinfo.gamefolder, Info_ValueForKey( info, "gamedir" )) != 0 )
		return true; // consumed, not listed

	FOR_EACH_VEC( m_Servers, i )
	{
		if( !EngFuncs::NET_CompareAdr( &m_Servers[i].adr, &adr ))
			return true; // already listed; keep first ping
	}

	srv_t s;
	memset( &s, 0, sizeof( s ));
	s.adr = adr;
	Q_strncpy( s.name, Info_ValueForKey( info, "host" ), sizeof( s.name ));
	Q_strncpy( s.map, Info_ValueForKey( info, "map" ), sizeof( s.map ));
	s.numcl = atoi( Info_ValueForKey( info, "numcl" ));
	s.maxcl = atoi( Info_ValueForKey( info, "maxcl" ));
	snprintf( s.players, sizeof( s.players ), "%d / %d", s.numcl, s.maxcl );
	s.password = !strcmp( Info_ValueForKey( info, "password" ), "1" );
	s.isGoldSrc = !strcmp( Info_ValueForKey( info, "gs" ), "1" );
	s.ping = bound( 0.0f, (float)( EngFuncs::DoubleTime() - m_flRefreshTime ), 9.999f );
	snprintf( s.pingstr, sizeof( s.pingstr ), "%.f ms", s.ping * 1000 );

	// keep the list ping-sorted as results trickle in
	int at = m_Servers.Count();
	for( int i = 0; i < m_Servers.Count(); i++ )
	{
		if( s.ping < m_Servers[i].ping )
		{
			at = i;
			break;
		}
	}
	m_Servers.InsertBefore( at, s );
	if( at <= m_iSel && m_Servers.Count() > 1 )
		m_iSel++;

	return true;
}

void CMenuContServers::Connect( srv_t &s )
{
	const char *sadr = EngFuncs::NET_AdrToString( s.adr );
	EngFuncs::ClientCmdF( false, "connect \"%s\" \"%s\"\n", sadr, s.isGoldSrc ? "gs" : "49" );
	UI_ConnectionProgress_Connect( "" );
}

void CMenuContServers::JoinSelected()
{
	if( !m_Servers.IsValidIndex( m_iSel ))
		return;

	srv_t &s = m_Servers[m_iSel];

	if( s.password )
	{
		m_PendingJoin = s;
		m_szPassword[0] = 0;
		m_bAskPassword = true;
		UI_EnableTextInput( true );
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_KEY] );
		return;
	}

	EngFuncs::CvarSetString( "password", "" );
	Connect( s );
}

void CMenuContServers::EnsureVisible()
{
	const int view = CONTENT_BOTTOM - CONTENT_TOP;
	const int rowH = ROW_H + 4;
	const int y = m_iSel * rowH;

	if( y - m_flScrollTarget < 6 )
		m_flScrollTarget = Q_max( 0, y - 6 );
	else if( y + ROW_H - m_flScrollTarget > view - 6 )
		m_flScrollTarget = y + ROW_H - view + 6;
}

int CMenuContServers::RowAtCursor()
{
	FOR_EACH_VEC( m_Servers, i )
	{
		int x = MARGIN, y = CONTENT_TOP + i * ( ROW_H + 4 ) - (int)m_flScroll;
		int w = ListW(), h = ROW_H;
		UI_ScaleCoords( x, y, w, h );
		if( UI_CursorInRect( x, y, w, h ))
			return i;
	}
	return -1;
}

bool CMenuContServers::KeyDown( int key )
{
	if( m_bAskPassword )
	{
		if( UI::Key::IsEscape( key ))
		{
			m_bAskPassword = false;
			UI_EnableTextInput( false );
			EngFuncs::PlayLocalSound( uiStatic.sounds[SND_OUT] );
		}
		else if( UI::Key::IsEnter( key ) && key != K_A_BUTTON )
		{
			m_bAskPassword = false;
			UI_EnableTextInput( false );
			EngFuncs::CvarSetString( "password", m_szPassword );
			Connect( m_PendingJoin );
		}
		else if( key == K_BACKSPACE )
		{
			const int len = strlen( m_szPassword );
			if( len > 0 )
				m_szPassword[len - 1] = 0;
		}
		return true;
	}

	if( UI::Key::IsEscape( key ))
	{
		Hide();
		return true;
	}

	if( UI::Key::IsUpArrow( key ) || UI::Key::IsDownArrow( key ))
	{
		const int dir = UI::Key::IsDownArrow( key ) ? 1 : -1;
		const int next = m_iSel + dir;
		if( next >= 0 && next < m_Servers.Count( ))
		{
			m_iSel = next;
			EnsureVisible();
			EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
		}
		return true;
	}

	if( UI::Key::IsPageUp( key ))
	{
		SetSource( false );
		return true;
	}
	if( UI::Key::IsPageDown( key ))
	{
		SetSource( true );
		return true;
	}

	if( key == K_MWHEELUP || key == K_MWHEELDOWN )
	{
		const int view = CONTENT_BOTTOM - CONTENT_TOP;
		const float maxScroll = Q_max( 0, m_Servers.Count() * ( ROW_H + 4 ) - view );
		m_flScrollTarget = bound( 0.0f, m_flScrollTarget + ( key == K_MWHEELDOWN ? 90.0f : -90.0f ), maxScroll );
		return true;
	}

	if( key == K_X_BUTTON || key == 'x' )
	{
		Refresh();
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
		return true;
	}

	if( UI::Key::IsEnter( key ))
	{
		JoinSelected();
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
			{
				m_iSel = hit;
				EnsureVisible();
				EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
			}
		}
		return true;
	}

	return CMenuFramework::KeyDown( key );
}

bool CMenuContServers::KeyUp( int key )
{
	if( m_bAskPassword )
		return true;

	if( key == K_MOUSE1 )
	{
		const bool was = m_bMousePressed;
		m_bMousePressed = false;
		if( was && RowAtCursor() == m_iSel )
		{
			JoinSelected();
			return true;
		}
	}
	return CMenuFramework::KeyUp( key );
}

bool CMenuContServers::MouseMove( int x, int y )
{
	if( !m_bAskPassword )
	{
		const int hit = RowAtCursor();
		if( hit >= 0 && hit != m_iSel )
		{
			m_iSel = hit;
			EnsureVisible();
		}
	}
	return true;
}

void CMenuContServers::Draw()
{
	static CImage noBackdrop;
	DrawBackdrop( noBackdrop );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"SERVERS", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// source tabs, LB/RB style like the config tab bar
	const int tabH = 15 * uiStatic.scaleY;
	const int tabY = 160 * uiStatic.scaleY;
	static const char *sources[2] = { "INTERNET", "LAN" };
	int x = tx;

	x += DrawGlyph( GLYPH_LB, x, tabY - 2 * uiStatic.scaleY, tabH * 1.3f ) + 18 * uiStatic.scaleX;
	for( int i = 0; i < 2; i++ )
	{
		const bool active = ( m_bLan == ( i == 1 ));
		const int wide = g_FontMgr->GetTextWideScaled( fontSmall, sources[i], tabH );

		UI_DrawString( fontSmall, x, tabY, wide + 4, tabH * 1.45f, sources[i],
			active ? clrInk : clrInkFaint, tabH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
		if( active )
			UI_FillRect( x, tabY + tabH + 8 * uiStatic.scaleY, wide, 3 * uiStatic.scaleY, clrAccent );

		x += wide + 34 * uiStatic.scaleX;
	}
	DrawGlyph( GLYPH_RB, x, tabY - 2 * uiStatic.scaleY, tabH * 1.3f );

	// column captions
	const int listW = ListW();
	const int colMap = listW - 560;
	const int colPlayers = listW - 320;
	const int colPing = listW - 150;
	const int capH = 11 * uiStatic.scaleY;
	const int capY = ( CONTENT_TOP - 24 + uiStatic.yOffset ) * uiStatic.scaleY;

	UI_DrawString( fontSmall, ( MARGIN + 22 ) * uiStatic.scaleX, capY, 300 * uiStatic.scaleX, capH * 1.45f,
		"SERVER", clrInkFaint, capH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, ( MARGIN + colMap ) * uiStatic.scaleX, capY, 200 * uiStatic.scaleX, capH * 1.45f,
		"MAP", clrInkFaint, capH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, ( MARGIN + colPlayers ) * uiStatic.scaleX, capY, 140 * uiStatic.scaleX, capH * 1.45f,
		"PLAYERS", clrInkFaint, capH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, ( MARGIN + colPing ) * uiStatic.scaleX, capY, 120 * uiStatic.scaleX, capH * 1.45f,
		"PING", clrInkFaint, capH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// smooth scroll
	const float dt = gpGlobals->frametime;
	m_flScroll += ( m_flScrollTarget - m_flScroll ) * bound( 0.0f, dt * 14.0f, 1.0f );

	const int clipTop = ( CONTENT_TOP + uiStatic.yOffset ) * uiStatic.scaleY;
	const int clipBottom = clipTop + ( CONTENT_BOTTOM - CONTENT_TOP ) * uiStatic.scaleY;
	EngFuncs::PIC_EnableScissor( 0, clipTop, ScreenWidth, clipBottom - clipTop );

	FOR_EACH_VEC( m_Servers, i )
	{
		const srv_t &s = m_Servers[i];
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

		const int nameH = 16 * uiStatic.scaleY;
		const int cellH = 14 * uiStatic.scaleY;
		const int cy = ry + ( rh - nameH ) / 2;

		char name[96];
		snprintf( name, sizeof( name ), "%s%s", s.password ? "* " : "", s.name[0] ? s.name : "(unnamed)" );
		UI_DrawString( fontItem, rx + 22 * uiStatic.scaleX, cy, ( colMap - 40 ) * uiStatic.scaleX, nameH * 1.45f,
			name, sel ? clrInk : clrInkDim, nameH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );

		UI_DrawString( fontBody, rx + colMap * uiStatic.scaleX, ry + ( rh - cellH ) / 2, 220 * uiStatic.scaleX,
			cellH * 1.45f, s.map, sel ? clrInk : clrInkDim, cellH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );
		UI_DrawString( fontBody, rx + colPlayers * uiStatic.scaleX, ry + ( rh - cellH ) / 2, 140 * uiStatic.scaleX,
			cellH * 1.45f, s.players, sel ? clrInk : clrInkDim, cellH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
		UI_DrawString( fontBody, rx + colPing * uiStatic.scaleX, ry + ( rh - cellH ) / 2, 120 * uiStatic.scaleX,
			cellH * 1.45f, s.pingstr, sel ? clrInk : clrInkDim, cellH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	}

	EngFuncs::PIC_DisableScissor();

	// empty states
	if( !m_Servers.Count( ))
	{
		const int msgH = 14 * uiStatic.scaleY;
		const bool searching = uiStatic.realTime - m_flQueryTime < 5000;
		UI_DrawString( fontBody, 0, ScreenHeight * 0.45f, ScreenWidth, msgH * 1.45f,
			searching ? "Searching for servers..." : "No servers found - press X to refresh",
			clrInkDim, msgH, QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL );
	}

	CMenuFramework::Draw();

	// password overlay
	if( m_bAskPassword )
	{
		UI_FillRect( 0, 0, ScreenWidth, ScreenHeight, 0x96000000 );

		const int pw = 620 * uiStatic.scaleX;
		const int ph = 170 * uiStatic.scaleY;
		const int px = ( ScreenWidth - pw ) / 2;
		const int py = ( ScreenHeight - ph ) / 2;

		UI_FillRect( px, py, pw, ph, 0xF20E1014 );
		UI_DrawRectangleExt( px, py, pw, ph, 0x28FFFFFF, 1 );
		UI_FillRect( px, py, pw, 3 * uiStatic.scaleY, clrAccent );

		const int lineH = 15 * uiStatic.scaleY;
		UI_DrawString( fontSmall, px, py + 26 * uiStatic.scaleY, pw, lineH * 1.45f,
			"PASSWORD REQUIRED", clrAccent, 12 * uiStatic.scaleY, QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL );

		// masked entry with caret
		char masked[40];
		int n = strlen( m_szPassword );
		n = Q_min( n, (int)sizeof( masked ) - 3 );
		memset( masked, '*', n );
		masked[n] = ( uiStatic.realTime / 280 ) & 1 ? '_' : ' ';
		masked[n + 1] = 0;
		UI_DrawString( fontItem, px, py + 64 * uiStatic.scaleY, pw, 18 * uiStatic.scaleY * 1.45f,
			masked, clrInk, 18 * uiStatic.scaleY, QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL );

		const int hintH = 13 * uiStatic.scaleY;
		UI_DrawString( fontHint, px, py + 118 * uiStatic.scaleY, pw, hintH * 1.45f,
			"Enter joins - Esc cancels", clrInkDim, hintH, QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL );
		return;
	}

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Join" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
		{ GLYPH_LB, GLYPH_RB, "Source" },
		{ GLYPH_X, GLYPH_COUNT, "Refresh" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ), "* = PASSWORD PROTECTED" );
}

void CMenuContServers::Char( int ch )
{
	// route typing into the password overlay
	if( m_bAskPassword )
	{
		if( ch < 32 )
			return;
		const int len = strlen( m_szPassword );
		if( len < (int)sizeof( m_szPassword ) - 1 )
		{
			m_szPassword[len] = ch;
			m_szPassword[len + 1] = 0;
		}
		return;
	}
	CMenuFramework::Char( ch );
}

ADD_MENU( menu_continuum_servers, CMenuContServers, UI_ContServers_Menu );

// the engine's server-list callback offers results here first; the stock
// browser (still compiled in) gets them only when our screen isn't up
bool UI_ContServers_AddServer( netadr_t adr, const char *info )
{
	if( !menu_continuum_servers )
		return false;
	return menu_continuum_servers->AddServer( adr, info );
}

/*
====================
host game: name, map, max players, start
====================
*/
class CMenuContHostGame : public CMenuFramework
{
public:
	CMenuContHostGame() : CMenuFramework( "CMenuContHostGame" ) { }

	bool KeyDown( int key ) override;
	void Draw() override;
	void Show() override;

private:
	void _Init() override;
	void _VidInit() override;

	void RefreshMaps();
	void Begin();

	CContTextRow serverName;
	CContDropdownRow mapRow;
	CContSpinRow maxPlayers;
	CContTextRow password;
	CContButton start;

	struct map_t
	{
		char name[64];
	};
	CUtlVector<map_t> m_Maps;
	const char *m_szMapLabels[CContDropdownRow::MAX_OPTIONS];
};

void CMenuContHostGame::_Init()
{
	serverName.SetNameAndStatus( "Server Name", NULL );
	serverName.szHint = "Shown in the server browser";
	serverName.Setup( "hostname", 48 );

	mapRow.SetNameAndStatus( "Map", NULL );

	static const char *playerLabels[] = { "2", "4", "6", "8", "12", "16", "24", "32" };
	static const float playerValues[] = { 2, 4, 6, 8, 12, 16, 24, 32 };
	maxPlayers.SetNameAndStatus( "Max Players", NULL );
	maxPlayers.Setup( "maxplayers", playerLabels, playerValues, 8, 3 );

	password.SetNameAndStatus( "Password", NULL );
	password.szHint = "Leave empty for a public server";
	password.Setup( "sv_password", 31 );

	start.SetNameAndStatus( "Start Server", NULL );
	start.szHint = "Starts a deathmatch listen server";
	start.onReleased = VoidCb( &CMenuContHostGame::Begin );

	AddItem( serverName );
	AddItem( mapRow );
	AddItem( maxPlayers );
	AddItem( password );
	AddItem( start );
}

void CMenuContHostGame::_VidInit()
{
	VidInitFonts();

	const int itemH = 56, gap = 6;
	int y = 280;

	serverName.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	mapRow.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	maxPlayers.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	password.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	start.SetRect( MARGIN, y, ROW_W, itemH );
}

void CMenuContHostGame::RefreshMaps()
{
	char *afile;

	m_Maps.RemoveAll();

	if( !EngFuncs::CreateMapsList( 1 ) || ( afile = (char *)EngFuncs::COM_LoadFile( "maps.lst", NULL )) == NULL )
	{
		mapRow.SetOptions( m_szMapLabels, 0 );
		start.SetGrayed( true );
		return;
	}

	map_t random;
	Q_strncpy( random.name, "(random map)", sizeof( random.name ));
	m_Maps.AddToTail( random );

	char *pfile = afile;
	char token[64];
	while(( pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ))) != NULL )
	{
		if( m_Maps.Count() >= (int)CContDropdownRow::MAX_OPTIONS )
			break;

		map_t m;
		Q_strncpy( m.name, token, sizeof( m.name ));
		m_Maps.AddToTail( m );

		// second token on the line is the map title; the dropdown shows names
		pfile = EngFuncs::COM_ParseFile( pfile, token, sizeof( token ));
		if( !pfile )
			break;
	}
	EngFuncs::COM_FreeFile( afile );

	FOR_EACH_VEC( m_Maps, i )
		m_szMapLabels[i] = m_Maps[i].name;

	mapRow.SetOptions( m_szMapLabels, m_Maps.Count( ));
	mapRow.SetApplied( 0 );
	start.SetGrayed( m_Maps.Count() < 2 );
}

void CMenuContHostGame::Show()
{
	CMenuFramework::Show();
	serverName.Reload();
	password.Reload();
	RefreshMaps();
}

void CMenuContHostGame::Begin()
{
	int item = mapRow.iPending;

	if( m_Maps.Count() < 2 )
		return;

	if( item <= 0 || item >= m_Maps.Count( ))
		item = EngFuncs::RandomLong( 1, m_Maps.Count() - 1 );

	const char *mapName = m_Maps[item].name;

	if( !EngFuncs::IsMapValid( mapName ))
		return;

	// commit any in-progress edits
	EngFuncs::CvarSetString( "hostname", serverName.GetBuffer( ));
	EngFuncs::CvarSetString( "sv_password", password.GetBuffer( ));

	if( EngFuncs::GetCvarFloat( "host_serverstate" ))
	{
		if( EngFuncs::GetCvarFloat( "maxplayers" ) == 1.0f )
			EngFuncs::HostEndGame( "end of the game" );
		else
			EngFuncs::HostEndGame( "starting new server" );
	}

	EngFuncs::CvarSetValue( "deathmatch", 1.0f );
	EngFuncs::PlayBackgroundTrack( NULL, NULL );

	// match the stock create-game flow: exec the listen-server config, then
	// wait a few frames for a clean shutdown before latching the new map
	EngFuncs::ClientCmdF( true, "exec %s\n", EngFuncs::GetCvarString( "lservercfgfile" ));

	const int players = bound( 2, (int)EngFuncs::GetCvarFloat( "maxplayers" ), 32 );

	char escaped[256];
	Com_EscapeCommand( escaped, mapName, sizeof( escaped ));
	EngFuncs::ClientCmdF( false,
		"disconnect;menu_connectionprogress localserver;wait;wait;wait;maxplayers %i;latch;map %s\n",
		players, escaped );
}

bool CMenuContHostGame::KeyDown( int key )
{
	// editing rows and the open map dropdown own Esc (base hides otherwise)
	if( UI::Key::IsEscape( key ))
	{
		if( serverName.IsEditing( ))
			return serverName.KeyDown( key );
		if( password.IsEditing( ))
			return password.KeyDown( key );
		if( mapRow.bOpen )
			return mapRow.KeyDown( key );
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

void CMenuContHostGame::Draw()
{
	static CImage noBackdrop;
	DrawBackdrop( noBackdrop );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"HOST GAME", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		"LISTEN SERVER", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	CMenuFramework::Draw();

	// the dropdown's pending/applied split is for deferred video modes; here
	// the choice simply IS the value, so keep them in sync (no '*' marker)
	mapRow.AcceptPending();
	mapRow.DrawPopup();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Select" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ));
}

ADD_MENU( menu_continuum_hostgame, CMenuContHostGame, UI_ContHostGame_Menu );
