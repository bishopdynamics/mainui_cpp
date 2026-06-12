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

// the screens, shown via these (UI_Main_Menu lives in RootMenu.cpp)
void UI_ContGamePicker_Menu( void );
void UI_ContGamePage_Menu( void );
void UI_ContConfig_Menu( void );

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

struct LegendEntry
{
	EGlyph glyph;
	EGlyph glyph2; // GLYPH_COUNT for none
	const char *text;
};
void DrawLegend( const LegendEntry *entries, int count, const char *rightText = NULL );

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

	const char *szHint;
	const char *szValue;  // optional right-aligned value (spinner-style rows)
	const char *szBadge;  // optional small amber badge after label ("RESTART")
	bool bCaution;        // amber accent instead of orange

protected:
	float FocusT(); // eased focus-in progress 0..1
};
}

#endif // CONTINUUM_H
