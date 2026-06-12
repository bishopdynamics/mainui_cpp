/*
Continuum.h -- shared theme, helpers and widgets for the Continuum menu
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
#pragma once
#ifndef CONTINUUM_H
#define CONTINUUM_H

#include "Framework.h"
#include "BaseMenu.h"
#include "Utils.h"
#include "Image.h"
#include "FontManager.h"
#include "keydefs.h"

void UI_MainClassic_Menu( void ); // the stock main menu (menus/Main.cpp)

// the screens, shown via these (UI_Main_Menu lives in RootMenu.cpp)
void UI_ContGamePicker_Menu( void );
void UI_ContGamePage_Menu( void );
void UI_ContConfig_Menu( void );
void UI_ContBindings_Menu( void );
void UI_ContGamepadAxes_Menu( void );
void UI_ContMultiplayer_Menu( void );
void UI_ContLoadGame_Menu( void );
void UI_ContSaveGame_Menu( void );
void UI_ContCharacter_Menu( void );

// the engine's server-list callback feeds the Continuum browser while it's
// on screen; returns false when the stock browser should take the result
bool UI_ContServers_AddServer( netadr_t adr, const char *info );

namespace Cont
{
// palette, 0xAARRGGBB like the rest of mainui
const unsigned int clrAccent     = 0xFFFFA31A;
const unsigned int clrAccentSoft = 0x2EFFA31A;
const unsigned int clrInk        = 0xFFE8E6E1;
const unsigned int clrInkDim     = 0xFF9A978F;
const unsigned int clrInkFaint   = 0xFF6E6C66;
const unsigned int clrBg         = 0xFF0B0D11;
const unsigned int clrPanel      = 0xB414171D;
const unsigned int clrCaution    = 0xFFE8B53F;

// layout in logical units (768-high space, uiStatic.width wide)
const int MARGIN   = 100;
const int LEGEND_H = 56;

// Michroma display fonts + body sans, created per video mode
extern HFont fontBrand;  // ~36
extern HFont fontTitle;  // ~26
extern HFont fontItem;   // ~19
extern HFont fontSmall;  // ~12 section headers
extern HFont fontBody;   // ~16 sans
extern HFont fontHint;   // ~13 sans
void VidInitFonts( void );

float EaseOutCubic( float t );

// render-space helpers
void DrawPicAspectFit( int x, int y, int w, int h, CImage &pic, unsigned int color = 0xFFFFFFFF );
void DrawBackdrop( CImage &pic ); // full-screen game-art backdrop (or flat bg)
// word-wrapped multi-line text ('\n' respected); returns y below the last line
int DrawWrappedText( HFont font, int x, int y, int w, int lineH, const char *text, unsigned int color );

// scrolling viewports: FillRGBA ignores the engine scissor, so scrolled rows
// skip drawing entirely when outside these render-space bounds (0/0 = off)
void SetRowClip( int top, int bottom );
bool RowClipped( int y, int h );

// controller glyphs (gfx/shell/continuum/glyphs/<style>/<glyph>.png)
enum EGlyph
{
	GLYPH_A = 0,
	GLYPH_B,
	GLYPH_X,
	GLYPH_Y,
	GLYPH_LB,
	GLYPH_RB,
	GLYPH_COUNT
};
const char *GlyphStyle( void );        // resolved style: xbox/ps/switch/deck/kb
int DrawGlyph( EGlyph g, int x, int y, int h ); // returns advance width
int GlyphWidth( EGlyph g, int h );     // width DrawGlyph would use; 0 if missing
EGlyph KeyToGlyph( int key );          // engine keynum -> glyph, GLYPH_COUNT if none

struct LegendEntry
{
	EGlyph glyph;
	EGlyph glyph2; // GLYPH_COUNT for none
	const char *text;
};
void DrawLegend( const LegendEntry *entries, int count, const char *rightText = NULL );

// mouse support for the legend bar: if the cursor sits on one of the entries
// drawn by the last DrawLegend call, returns the engine key that entry stands
// for (K_ENTER, K_ESCAPE, K_PGUP/K_PGDN, K_X_BUTTON, ...), else 0. Screens
// feed the result back into their own KeyDown.
int LegendClickKey( void );

// per-game menu art lookup; returns false (and leaves pic empty) if missing
bool GameArt( const char *folder, CImage &pic );
bool GameBackdrop( const char *folder, CImage &pic );

// small shared UI textures (white, tint at draw time)
CImage &PillPic( void );        // rounded capsule, 2:1
CImage &DotPic( void );         // circle
CImage &ChipCurrentPic( void ); // baked rounded CURRENT tag

// Mockup-styled list button: left accent edge + Michroma label, hint text
// slides out under the label while focused. Left/right are NOT consumed, so
// holders can use them for cursor movement where it makes sense.
class CContButton : public CMenuBaseItem
{
public:
	typedef CMenuBaseItem BaseClass;
	CContButton();

	bool KeyUp( int key ) override;
	bool KeyDown( int key ) override;
	void Draw() override;

	// settings rows override this; plain buttons ignore it
	virtual void ResetDefault() { }

	// scrolled layout: same math as CalcPosition/CalcSizes but WITHOUT the
	// "negative position means bottom-anchored" convention, so rows can sit
	// partially above the viewport while a list scrolls
	void SetScrolledRect( int x, int y, int w, int h );

	const char *szHint;
	const char *szValue;  // optional right-aligned value (spinner-style rows)
	const char *szBadge;  // optional small amber badge after label ("RESTART")
	const char *szCard;   // optional long explainer, shown in a side panel while focused
	const char *szCardTitle;
	bool bCaution;        // amber accent instead of orange
	bool bValueArrows;    // draw < > around szValue while focused (off for text rows)

protected:
	float FocusT(); // eased focus-in progress 0..1
};

/*
====================
shared settings-row widgets, used by the Configuration tabs and the other
Continuum screens (gamepad options, multiplayer). All methods are in-class
so the header stays the single definition.
====================
*/
// section header, skipped by the cursor
class CContHeader : public CContButton
{
public:
	CContHeader() { iFlags |= QMF_INACTIVE; }

	void Draw() override
	{
		if( RowClipped( m_scPos.y, m_scSize.h ))
			return;

		const int h = 12 * uiStatic.scaleY;
		UI_DrawString( fontSmall, m_scPos.x + 6 * uiStatic.scaleX, m_scPos.y + m_scSize.h - h * 1.6f,
			m_scSize.w, h * 1.45f, szName, clrAccent, h, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	}
};

class CContToggleRow : public CContButton
{
public:
	void Setup( const char *cv, float def )
	{
		szCvar = cv;
		flDefault = def;
	}

	void Reload() override { bOn = EngFuncs::GetCvarFloat( szCvar ) != 0.0f; }
	void ResetDefault() override { EngFuncs::CvarSetValue( szCvar, flDefault ); Reload(); }

	bool KeyDown( int key ) override
	{
		if( UI::Key::IsLeftArrow( key ) || UI::Key::IsRightArrow( key ) || UI::Key::IsEnter( key )
			|| ( key == K_MOUSE1 && UI_CursorInRect( m_scPos, m_scSize )))
		{
			bOn = !bOn;
			EngFuncs::CvarSetValue( szCvar, bOn ? 1.0f : 0.0f );
			PlayLocalSound( uiStatic.sounds[SND_MOVE] );
			_Event( QM_CHANGED );
			return true;
		}
		return CContButton::KeyDown( key );
	}

	void Draw() override
	{
		if( RowClipped( m_scPos.y, m_scSize.h ))
			return;

		CContButton::Draw();

		// toggle pill, right-aligned
		const int ph = 20 * uiStatic.scaleY;
		const int pw = 40 * uiStatic.scaleX;
		const int px = m_scPos.x + m_scSize.w - pw - 30 * uiStatic.scaleX;
		const int py = m_scPos.y + ( m_scSize.h - ph ) / 2;
		const int dot = ph - 6 * uiStatic.scaleY;
		const bool grayed = FBitSet( iFlags, QMF_GRAYED );

		UI_DrawPic( px, py, pw, ph, bOn && !grayed ? ( bCaution ? clrCaution : clrAccent ) : 0x50FFFFFF,
			PillPic(), QM_DRAWTRANS );
		const int dx = bOn ? px + pw - dot - 3 * uiStatic.scaleX : px + 3 * uiStatic.scaleX;
		UI_DrawPic( dx, py + 3 * uiStatic.scaleY, dot, dot, grayed ? clrInkDim : 0xFFFFFFFF,
			DotPic(), QM_DRAWTRANS );
	}

	const char *szCvar;
	float flDefault;
	bool bOn;
};

class CContSpinRow : public CContButton
{
public:
	CContSpinRow() : nCount( 0 ), iIndex( 0 ), szCvar( NULL ), szLabels( NULL ),
		flValues( NULL ), szValues( NULL ), iDefault( 0 ) { }

	void Setup( const char *cv, const char **labels, const float *fvals, int count, int defIdx )
	{
		szCvar = cv;
		szLabels = labels;
		flValues = fvals;
		szValues = NULL;
		nCount = count;
		iDefault = defIdx;
	}

	void SetupString( const char *cv, const char **labels, const char **svals, int count, int defIdx )
	{
		szCvar = cv;
		szLabels = labels;
		szValues = svals;
		flValues = NULL;
		nCount = count;
		iDefault = defIdx;
	}

	void Reload() override
	{
		if( !szCvar ) return;

		iIndex = iDefault;
		if( szValues )
		{
			const char *v = EngFuncs::GetCvarString( szCvar );
			for( int i = 0; i < nCount; i++ )
				if( !stricmp( v, szValues[i] )) { iIndex = i; break; }
		}
		else
		{
			const float v = EngFuncs::GetCvarFloat( szCvar );
			float best = 1e9f;
			for( int i = 0; i < nCount; i++ )
			{
				const float d = fabs( v - flValues[i] );
				if( d < best ) { best = d; iIndex = i; }
			}
		}
		szValue = szLabels[iIndex];
	}

	virtual void Write()
	{
		if( !szCvar ) return;
		if( szValues )
			EngFuncs::CvarSetString( szCvar, szValues[iIndex] );
		else
			EngFuncs::CvarSetValue( szCvar, flValues[iIndex] );
	}

	void ResetDefault() override
	{
		iIndex = iDefault;
		szValue = szLabels[iIndex];
		Write();
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
			const int next = iIndex + dir;
			if( next >= 0 && next < nCount )
			{
				iIndex = next;
				szValue = szLabels[iIndex];
				Write();
				PlayLocalSound( uiStatic.sounds[SND_MOVE] );
				_Event( QM_CHANGED );
			}
			return true;
		}
		return CContButton::KeyDown( key );
	}

	const char *szCvar;
	const char **szLabels;
	const float *flValues;
	const char **szValues;
	int nCount;
	int iIndex;
	int iDefault;
};

// dropdown row: A opens an overlay list, left/right nudges without opening.
// Changes are PENDING until the screen applies them (video settings get the
// apply + 15 s confirm/revert treatment).
class CContDropdownRow : public CContButton
{
public:
	enum { MAX_OPTIONS = 64 };

	CContDropdownRow() : nCount( 0 ), iApplied( 0 ), iPending( 0 ),
		bOpen( false ), iHover( 0 ) { }

	void SetOptions( const char **labels, int count )
	{
		nCount = Q_min( count, (int)MAX_OPTIONS );
		for( int i = 0; i < nCount; i++ )
			m_szOptions[i] = labels[i];
		Sync();
	}

	void SetApplied( int idx )
	{
		iApplied = iPending = bound( 0, idx, nCount - 1 );
		Sync();
	}

	bool HasPending() const { return iPending != iApplied; }
	void AcceptPending() { iApplied = iPending; Sync(); }
	void RevertPending() { iPending = iApplied; Sync(); }

	bool KeyDown( int key ) override
	{
		if( bOpen )
		{
			if( UI::Key::IsUpArrow( key ))
				iHover = ( iHover + nCount - 1 ) % nCount;
			else if( UI::Key::IsDownArrow( key ))
				iHover = ( iHover + 1 ) % nCount;
			else if( UI::Key::IsEnter( key ))
			{
				iPending = iHover;
				bOpen = false;
				Sync();
			}
			else if( UI::Key::IsEscape( key ))
				bOpen = false;
			else if( key == K_MOUSE1 )
			{
				if( UI_CursorInRect( m_iPopX, m_iPopY, m_iPopW, m_iPopVisible * m_iPopItemH ))
				{
					const int idx = m_iPopFirst + ( uiStatic.cursorY - m_iPopY ) / m_iPopItemH;
					if( idx >= 0 && idx < nCount )
					{
						iPending = idx;
						Sync();
					}
				}
				bOpen = false; // click inside selects, click outside dismisses
			}

			PlayLocalSound( uiStatic.sounds[SND_MOVE] );
			return true; // swallow everything while open
		}

		if( UI::Key::IsEnter( key ) || ( UI::Key::IsMouse( key ) && UI_CursorInRect( m_scPos, m_scSize )))
		{
			bOpen = true;
			iHover = iPending;
			PlayLocalSound( uiStatic.sounds[SND_MOVE] );
			return true;
		}

		if( UI::Key::IsLeftArrow( key ) || UI::Key::IsRightArrow( key ))
		{
			const int next = iPending + ( UI::Key::IsRightArrow( key ) ? 1 : -1 );
			if( next >= 0 && next < nCount )
			{
				iPending = next;
				Sync();
				PlayLocalSound( uiStatic.sounds[SND_MOVE] );
			}
			return true;
		}

		return false;
	}

	void Draw() override
	{
		Sync();
		CContButton::Draw();

		// pending marker
		if( HasPending( ))
		{
			const int h = 16 * uiStatic.scaleY;
			UI_DrawString( fontBody, m_scPos.x + m_scSize.w - 22 * uiStatic.scaleX, m_scPos.y + ( m_scSize.h - h ) / 2,
				20 * uiStatic.scaleX, h * 1.45f, "*", clrAccent, h, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
		}
	}

	// the screen calls this after everything else so the list overlays rows
	void DrawPopup()
	{
		if( !bOpen || !nCount )
			return;

		const int itemH = 30 * uiStatic.scaleY;
		const int visible = Q_min( nCount, 9 );
		const int w = 260 * uiStatic.scaleX;
		const int x = m_scPos.x + m_scSize.w - w;
		int y = m_scPos.y + m_scSize.h;

		// keep the hovered option in the window
		int first = bound( 0, iHover - visible / 2, Q_max( 0, nCount - visible ));

		// clamp to screen bottom
		if( y + visible * itemH > ScreenHeight )
			y = m_scPos.y - visible * itemH;

		m_iPopX = x;
		m_iPopY = y;
		m_iPopW = w;
		m_iPopItemH = itemH;
		m_iPopFirst = first;
		m_iPopVisible = visible;

		UI_FillRect( x - 2, y - 2, w + 4, visible * itemH + 4, 0xF20E1014 );
		UI_DrawRectangleExt( x - 2, y - 2, w + 4, visible * itemH + 4, 0x46FFFFFF, 1 );

		const int textH = 15 * uiStatic.scaleY;
		for( int i = 0; i < visible; i++ )
		{
			const int idx = first + i;
			const int ry = y + i * itemH;

			if( idx == iHover )
				UI_FillRect( x, ry, w, itemH, clrAccentSoft );
			if( idx == iHover )
				UI_FillRect( x, ry, 3 * uiStatic.scaleX, itemH, clrAccent );

			unsigned int color = idx == iApplied ? clrAccent : ( idx == iHover ? clrInk : clrInkDim );
			UI_DrawString( fontBody, x + 14 * uiStatic.scaleX, ry + ( itemH - textH ) / 2, w - 20 * uiStatic.scaleX,
				textH * 1.45f, m_szOptions[idx], color, textH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );
		}
	}

	bool bOpen;
	int nCount;
	int iApplied;
	int iPending;
	int iHover;

private:
	void Sync() { szValue = nCount ? m_szOptions[bound( 0, iPending, nCount - 1 )] : ""; }
	const char *m_szOptions[MAX_OPTIONS];
	int m_iPopX, m_iPopY, m_iPopW, m_iPopItemH, m_iPopFirst, m_iPopVisible;
};

class CContSliderRow : public CContButton
{
public:
	void Setup( const char *cv, float min, float max, float step, float def, int decimals = 1 )
	{
		szCvar = cv;
		flMin = min;
		flMax = max;
		flStep = step;
		flDefault = def;
		nDecimals = decimals;
	}

	// track geometry in render space, shared by Draw and mouse handling
	void TrackRect( int &tx, int &ty, int &tw, int &th )
	{
		tw = 170 * uiStatic.scaleX;
		tx = m_scPos.x + m_scSize.w - tw - 44 * uiStatic.scaleX - 30 * uiStatic.scaleX;
		th = 4 * uiStatic.scaleY;
		ty = m_scPos.y + m_scSize.h / 2 - th / 2;
	}

	virtual void SetValue( float v )
	{
		// snap to step
		v = flMin + (int)(( v - flMin ) / flStep + 0.5f ) * flStep;
		flValue = bound( flMin, v, flMax );
		EngFuncs::CvarSetValue( szCvar, flValue );
		_Event( QM_CHANGED );
	}

	void SetFromCursor()
	{
		int tx, ty, tw, th;
		TrackRect( tx, ty, tw, th );
		const float frac = bound( 0.0f, ( uiStatic.cursorX - tx ) / (float)tw, 1.0f );
		SetValue( flMin + frac * ( flMax - flMin ));
	}

	void Think() override
	{
		// drag: while pressed with the button held, follow the cursor
		if( m_bPressed && g_bCursorDown )
			SetFromCursor();
		CContButton::Think();
	}

	void Reload() override
	{
		flValue = bound( flMin, EngFuncs::GetCvarFloat( szCvar ), flMax );
	}

	void ResetDefault() override { EngFuncs::CvarSetValue( szCvar, flDefault ); Reload(); }

	bool KeyDown( int key ) override
	{
		if( UI::Key::IsLeftArrow( key ) || UI::Key::IsRightArrow( key ))
		{
			const float dir = UI::Key::IsRightArrow( key ) ? 1.0f : -1.0f;
			flValue = bound( flMin, flValue + dir * flStep, flMax );
			EngFuncs::CvarSetValue( szCvar, flValue );
			_Event( QM_CHANGED );
			return true;
		}

		if( key == K_MOUSE1 && UI_CursorInRect( m_scPos, m_scSize ))
		{
			m_bPressed = true;
			SetFromCursor();
			return true;
		}

		return CContButton::KeyDown( key );
	}

	void Draw() override
	{
		if( RowClipped( m_scPos.y, m_scSize.h ))
			return;

		CContButton::Draw();

		const int numH = 13 * uiStatic.scaleY;
		char num[16];
		snprintf( num, sizeof( num ), "%.*f", nDecimals, flValue );

		int tx, ty, tw, th;
		TrackRect( tx, ty, tw, th );
		const int cy = m_scPos.y + m_scSize.h / 2;
		const float frac = ( flValue - flMin ) / ( flMax - flMin );

		UI_FillRect( tx, ty, tw, th, 0x3CFFFFFF );
		UI_FillRect( tx, ty, tw * frac, th, bCaution ? clrCaution : clrAccent );

		const int knob = 13 * uiStatic.scaleY;
		UI_DrawPic( tx + tw * frac - knob / 2, cy - knob / 2, knob, knob, clrInk, DotPic(), QM_DRAWTRANS );

		UI_DrawString( fontHint, tx + tw + 12 * uiStatic.scaleX, cy - numH / 2 - 2, 44 * uiStatic.scaleX, numH * 1.45f,
			num, clrInkDim, numH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	}

	const char *szCvar;
	float flMin, flMax, flStep, flDefault, flValue;
	int nDecimals;
};

// invert-look toggle: UI face over the sign of a pitch cvar (m_pitch for
// the mouse, joy_pitch for the right stick)
class CContInvertRow : public CContToggleRow
{
public:
	void SetupSign( const char *cv ) { szCvar = cv; }

	void Reload() override { bOn = EngFuncs::GetCvarFloat( szCvar ) < 0.0f; }
	void ResetDefault() override
	{
		EngFuncs::CvarSetValue( szCvar, fabs( EngFuncs::GetCvarFloat( szCvar )));
		Reload();
	}

	bool KeyDown( int key ) override
	{
		if( UI::Key::IsLeftArrow( key ) || UI::Key::IsRightArrow( key ) || UI::Key::IsEnter( key ))
		{
			const float pitch = EngFuncs::GetCvarFloat( szCvar );
			EngFuncs::CvarSetValue( szCvar, -pitch );
			bOn = -pitch < 0.0f;
			PlayLocalSound( uiStatic.sounds[SND_MOVE] );
			return true;
		}
		return CContButton::KeyDown( key );
	}
};

// slider that drives several cvars at once with one magnitude, preserving
// each cvar's sign (joy look sensitivity, stick deadzones)
class CContMultiSliderRow : public CContSliderRow
{
public:
	enum { MAX_CVARS = 4 };

	void SetupMulti( const char **cvars, int count, float min, float max, float step, float def, int decimals = 0 )
	{
		nCvars = Q_min( count, (int)MAX_CVARS );
		for( int i = 0; i < nCvars; i++ )
			szCvars[i] = cvars[i];
		Setup( cvars[0], min, max, step, def, decimals );
	}

	void Reload() override
	{
		flValue = bound( flMin, fabs( EngFuncs::GetCvarFloat( szCvars[0] )), flMax );
	}

	void ResetDefault() override
	{
		flValue = flDefault;
		WriteAll();
	}

	void SetValue( float v ) override
	{
		v = flMin + (int)(( v - flMin ) / flStep + 0.5f ) * flStep;
		flValue = bound( flMin, v, flMax );
		WriteAll();
		_Event( QM_CHANGED );
	}

	bool KeyDown( int key ) override
	{
		if( UI::Key::IsLeftArrow( key ) || UI::Key::IsRightArrow( key ))
		{
			const float dir = UI::Key::IsRightArrow( key ) ? 1.0f : -1.0f;
			flValue = bound( flMin, flValue + dir * flStep, flMax );
			WriteAll();
			return true;
		}
		return CContSliderRow::KeyDown( key );
	}

private:
	void WriteAll()
	{
		for( int i = 0; i < nCvars; i++ )
		{
			const float sign = EngFuncs::GetCvarFloat( szCvars[i] ) < 0.0f ? -1.0f : 1.0f;
			EngFuncs::CvarSetValue( szCvars[i], flValue * sign );
		}
	}

	const char *szCvars[MAX_CVARS];
	int nCvars;
};
/*
====================
inline text-entry row: A starts editing, type, Enter commits to the cvar,
Esc reverts. Char events arrive through the holder while the row has focus.
Maintains a text cursor (left/right arrows, which the on-screen keyboard
drives via LB/RB) and mirrors the live text into osk_preview for the OSK.
====================
*/
class CContTextRow : public CContButton
{
public:
	CContTextRow() : szCvar( NULL ), m_iCursor( 0 ), m_bEditing( false )
	{
		m_szBuffer[0] = 0;
		bValueArrows = false; // text, not a spinner
	}

	void Setup( const char *cv, int maxLen )
	{
		szCvar = cv;
		m_iMaxLen = Q_min( maxLen, (int)sizeof( m_szBuffer ) - 1 );
	}

	void Reload() override
	{
		if( szCvar )
			Q_strncpy( m_szBuffer, EngFuncs::GetCvarString( szCvar ), m_iMaxLen + 1 );
		m_iCursor = strlen( m_szBuffer );
		StopEditing();
	}

	const char *GetBuffer() { return m_szBuffer; }

	bool IsEditing() const { return m_bEditing; }

	void StopEditing()
	{
		if( m_bEditing )
		{
			UI_EnableTextInput( false );
			EngFuncs::CvarSetString( "osk_preview", "" );
		}
		m_bEditing = false;
	}

	void Commit()
	{
		StopEditing();
		if( !szCvar )
			return;

		// player names must survive the engine's validation
		if( !strcmp( szCvar, "name" ) && !UI::Names::CheckIsNameValid( m_szBuffer ))
		{
			PlayLocalSound( uiStatic.sounds[SND_BUZZ] );
			Reload();
			return;
		}

		EngFuncs::CvarSetString( szCvar, m_szBuffer );
		PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
	}

	bool KeyDown( int key ) override
	{
		if( m_bEditing )
		{
			if( UI::Key::IsEscape( key ))
			{
				Reload();
				PlayLocalSound( uiStatic.sounds[SND_OUT] );
			}
			else if( UI::Key::IsEnter( key ) && key != K_A_BUTTON )
			{
				// keyboard Enter commits; pad A keeps typing via the OSK
				Commit();
			}
			else if( key == K_A_BUTTON )
			{
				return true;
			}
			else if( key == K_BACKSPACE )
			{
				if( m_iCursor > 0 )
				{
					const int len = strlen( m_szBuffer );
					const int prev = Con_UtfMoveLeft( m_szBuffer, m_iCursor );
					memmove( m_szBuffer + prev, m_szBuffer + m_iCursor, len - m_iCursor + 1 );
					m_iCursor = prev;
					SyncPreview();
				}
			}
			else if( key == K_LEFTARROW )
			{
				if( m_iCursor > 0 )
					m_iCursor = Con_UtfMoveLeft( m_szBuffer, m_iCursor );
			}
			else if( key == K_RIGHTARROW )
			{
				const int len = strlen( m_szBuffer );
				if( m_iCursor < len )
					m_iCursor = Con_UtfMoveRight( m_szBuffer, m_iCursor, len );
			}
			else if( key == K_MOUSE1 && !UI_CursorInRect( m_scPos, m_scSize ))
			{
				Commit(); // clicking away commits
				return false;
			}
			return true; // swallow navigation while editing
		}

		if( UI::Key::IsEnter( key ) || ( key == K_MOUSE1 && UI_CursorInRect( m_scPos, m_scSize )))
		{
			m_bEditing = true;
			m_iCursor = strlen( m_szBuffer );
			UI_EnableTextInput( true );
			SyncPreview();
			PlayLocalSound( uiStatic.sounds[SND_KEY] );
			return true;
		}

		return CContButton::KeyDown( key );
	}

	bool KeyUp( int key ) override
	{
		if( m_bEditing )
			return true;
		return CContButton::KeyUp( key );
	}

	void Char( int ch ) override
	{
		if( !m_bEditing || ch < 32 )
			return;

		const int len = strlen( m_szBuffer );
		if( len >= m_iMaxLen )
			return;

		memmove( m_szBuffer + m_iCursor + 1, m_szBuffer + m_iCursor, len - m_iCursor + 1 );
		m_szBuffer[m_iCursor++] = ch;
		SyncPreview();
	}

	void _Event( int ev ) override
	{
		// losing the cursor while typing keeps whatever was entered
		if( ev == QM_LOSTFOCUS && m_bEditing )
			Commit();
		CContButton::_Event( ev );
	}

	void Draw() override
	{
		if( m_bEditing )
		{
			// blinking caret at the cursor position
			const char caret = ( uiStatic.realTime / 280 ) & 1 ? '_' : ' ';
			snprintf( m_szDisplay, sizeof( m_szDisplay ), "%.*s%c%s",
				m_iCursor, m_szBuffer, caret, m_szBuffer + m_iCursor );
			szValue = m_szDisplay;
		}
		else
			szValue = m_szBuffer[0] ? m_szBuffer : "-";

		CContButton::Draw();
	}

	const char *szCvar;

private:
	void SyncPreview()
	{
		EngFuncs::CvarSetString( "osk_preview", m_szBuffer );
	}

	char m_szBuffer[64];
	char m_szDisplay[68];
	int m_iMaxLen;
	int m_iCursor;
	bool m_bEditing;
};

}

#endif // CONTINUUM_H
