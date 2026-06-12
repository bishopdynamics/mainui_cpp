/*
Continuum.cpp -- shared theme, helpers and widgets for the Continuum menu
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
#include "keydefs.h"

namespace Cont
{
HFont fontBrand;
HFont fontTitle;
HFont fontItem;
HFont fontSmall;
HFont fontBody;
HFont fontHint;

static cvar_t *ui_glyph_style;

void VidInitFonts( void )
{
	const float scale = uiStatic.scaleY;

	if( !ui_glyph_style )
		ui_glyph_style = EngFuncs::CvarRegister( "ui_glyph_style", "auto", FCVAR_ARCHIVE );

	// CFontBuilder dedups identical requests, cheap to call per screen
	fontBrand = CFontBuilder( "Michroma", 36 * scale, 500 ).Create();
	fontTitle = CFontBuilder( "Michroma", 26 * scale, 500 ).Create();
	fontItem  = CFontBuilder( "Michroma", 17 * scale, 500 ).Create();
	fontSmall = CFontBuilder( "Michroma", 12 * scale, 500 ).Create();
	fontBody  = CFontBuilder( "Trebuchet MS", 16 * scale, 500 ).Create();
	fontHint  = CFontBuilder( "Trebuchet MS", 13 * scale, 500 ).Create();
}

float EaseOutCubic( float t )
{
	t = bound( 0.0f, t, 1.0f );
	const float u = 1.0f - t;
	return 1.0f - u * u * u;
}

void DrawPicAspectFit( int x, int y, int w, int h, CImage &pic, unsigned int color )
{
	if( !pic.IsValid( ))
		return;

	const int pw = EngFuncs::PIC_Width( pic.Handle() );
	const int ph = EngFuncs::PIC_Height( pic.Handle() );
	if( pw <= 0 || ph <= 0 )
		return;

	int dw = w, dh = h;
	if( pw * h > ph * w ) // image wider than box: width-bound
		dh = (float)w * ph / pw;
	else
		dw = (float)h * pw / ph;

	UI_DrawPic( x + ( w - dw ) / 2, y + ( h - dh ) / 2, dw, dh, color, pic );
}

void DrawBackdrop( CImage &pic )
{
	if( pic.IsValid( ))
	{
		// asset is pre-blurred and pre-darkened; cover-fill distortion of a
		// blur is invisible, so just stretch
		UI_DrawPic( 0, 0, ScreenWidth, ScreenHeight, 0xFFFFFFFF, pic );
		// deepen the bottom so the legend bar reads
		UI_FillRect( 0, ScreenHeight * 0.86f, ScreenWidth, ScreenHeight * 0.14f + 1, 0x46000000 );
	}
	else
	{
		UI_FillRect( 0, 0, ScreenWidth, ScreenHeight, clrBg );
	}
}

static int g_iRowClipTop, g_iRowClipBottom;

void SetRowClip( int top, int bottom )
{
	g_iRowClipTop = top;
	g_iRowClipBottom = bottom;
}

bool RowClipped( int y, int h )
{
	if( g_iRowClipTop == 0 && g_iRowClipBottom == 0 )
		return false;
	// rows mostly outside the viewport skip entirely; their FillRGBA-based
	// parts (focus bar, badges, slider tracks) ignore the engine scissor
	return y + h * 0.6f < g_iRowClipTop || y + h * 0.4f > g_iRowClipBottom;
}

int DrawWrappedText( HFont font, int x, int y, int w, int lineH, const char *text, unsigned int color )
{
	char line[256];

	while( *text )
	{
		// hard line breaks
		if( *text == '\n' )
		{
			y += lineH * ( text[1] == '\n' ? 0.6f : 1.0f ); // blank line = paragraph gap
			text++;
			continue;
		}

		bool remaining = false;
		int end = g_FontMgr->CutText( font, text, lineH, w, false, true, NULL, &remaining );
		if( end <= 0 )
			end = 1;

		// cut before an explicit newline if one comes earlier
		for( int i = 0; i < end; i++ )
		{
			if( text[i] == '\n' )
			{
				end = i;
				break;
			}
		}

		Q_strncpy( line, text, Q_min( (size_t)end + 1, sizeof( line )));
		UI_DrawString( font, x, y, w, lineH * 1.45f, line, color, lineH, QM_LEFT | QM_TOP,
			ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );

		y += lineH * 1.45f;
		text += end;
		while( *text == ' ' )
			text++;
	}

	return y;
}

/*
====================
glyphs
====================
*/
static const char *g_szGlyphNames[GLYPH_COUNT] = { "a", "b", "x", "y", "lb", "rb" };
static CImage g_GlyphPics[GLYPH_COUNT];
static char g_szGlyphLoadedStyle[16];

const char *GlyphStyle( void )
{
	const char *style = ui_glyph_style ? ui_glyph_style->string : "auto";

	if( !strcmp( style, "auto" ))
	{
		// engine reports the raw SDL_GameControllerType of the active pad
		switch( (int)EngFuncs::GetCvarFloat( "joy_controller_type" ))
		{
		case 3:  // PS3
		case 4:  // PS4
		case 7:  // PS5
			return "ps";
		case 5:  // Switch Pro
		case 11: // Joy-Con left
		case 12: // Joy-Con right
		case 13: // Joy-Con pair
			return "switch";
		case 0:  // no controller seen
			return "kb";
		default: // Xbox of any age, and everything else
			return "xbox";
		}
	}

	return style;
}

static void LoadGlyphs( void )
{
	const char *style = GlyphStyle();

	if( !strcmp( g_szGlyphLoadedStyle, style ))
		return;

	for( int i = 0; i < GLYPH_COUNT; i++ )
	{
		char path[96];
		snprintf( path, sizeof( path ), "gfx/shell/continuum/glyphs/%s/%s.png", style, g_szGlyphNames[i] );
		g_GlyphPics[i].Load( path );
	}

	Q_strncpy( g_szGlyphLoadedStyle, style, sizeof( g_szGlyphLoadedStyle ));
}

int DrawGlyph( EGlyph g, int x, int y, int h )
{
	if( g < 0 || g >= GLYPH_COUNT )
		return 0;

	LoadGlyphs();

	CImage &pic = g_GlyphPics[g];
	if( !pic.IsValid( ))
		return 0;

	const int pw = EngFuncs::PIC_Width( pic.Handle() );
	const int ph = EngFuncs::PIC_Height( pic.Handle() );
	const int w = ph > 0 ? h * pw / ph : h;

	UI_DrawPic( x, y, w, h, 0xFFFFFFFF, pic );
	return w;
}

// last-drawn legend entry hitboxes, for LegendClickKey
static struct
{
	int x, y, w, h;
	EGlyph glyph;
	bool pair;
} g_LegendHits[8];
static int g_nLegendHits;

void DrawLegend( const LegendEntry *entries, int count, const char *rightText )
{
	const int barH = LEGEND_H * uiStatic.scaleY;
	const int y = ScreenHeight - barH;
	const int glyphH = 24 * uiStatic.scaleY;
	const int textH = 14 * uiStatic.scaleY;
	const int margin = MARGIN * uiStatic.scaleX;

	UI_FillRect( 0, y, ScreenWidth, barH, 0x6E000000 );
	UI_FillRect( 0, y, ScreenWidth, 1, 0x28FFFFFF );

	int x = margin;
	const int gy = y + ( barH - glyphH ) / 2;
	const int ty = y + ( barH - textH ) / 2;

	g_nLegendHits = 0;

	for( int i = 0; i < count; i++ )
	{
		const int startX = x;

		x += DrawGlyph( entries[i].glyph, x, gy, glyphH );
		if( entries[i].glyph2 != GLYPH_COUNT )
			x += DrawGlyph( entries[i].glyph2, x + 4 * uiStatic.scaleX, gy, glyphH ) + 4 * uiStatic.scaleX;

		x += 10 * uiStatic.scaleX;
		// UI_DrawString returns the rightmost x reached, not the width
		x = UI_DrawString( fontHint, x, ty, ScreenWidth, textH * 1.4f,
			entries[i].text, clrInkDim, textH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

		if( g_nLegendHits < (int)V_ARRAYSIZE( g_LegendHits ))
		{
			g_LegendHits[g_nLegendHits].x = startX - 8 * uiStatic.scaleX;
			g_LegendHits[g_nLegendHits].y = y;
			g_LegendHits[g_nLegendHits].w = x - startX + 16 * uiStatic.scaleX;
			g_LegendHits[g_nLegendHits].h = barH;
			g_LegendHits[g_nLegendHits].glyph = entries[i].glyph;
			g_LegendHits[g_nLegendHits].pair = entries[i].glyph2 != GLYPH_COUNT;
			g_nLegendHits++;
		}

		x += 34 * uiStatic.scaleX;
	}

	if( rightText )
	{
		int wide = g_FontMgr->GetTextWideScaled( fontSmall, rightText, textH );
		UI_DrawString( fontSmall, ScreenWidth - margin - wide, ty, wide + 4, textH * 1.4f,
			rightText, clrInkFaint, textH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	}
}

int LegendClickKey( void )
{
	for( int i = 0; i < g_nLegendHits; i++ )
	{
		if( !UI_CursorInRect( g_LegendHits[i].x, g_LegendHits[i].y, g_LegendHits[i].w, g_LegendHits[i].h ))
			continue;

		// a paired entry (LB+RB "Section") cycles forward
		if( g_LegendHits[i].pair )
			return K_PGDN;

		switch( g_LegendHits[i].glyph )
		{
		case GLYPH_A:  return K_ENTER;
		case GLYPH_B:  return K_ESCAPE;
		case GLYPH_X:  return K_X_BUTTON;
		case GLYPH_Y:  return K_Y_BUTTON;
		case GLYPH_LB: return K_PGUP;
		case GLYPH_RB: return K_PGDN;
		default: break;
		}
	}
	return 0;
}

/*
====================
game art lookup
====================
*/
static bool LoadGamePic( const char *folder, const char *suffix, CImage &pic )
{
	char path[96];
	snprintf( path, sizeof( path ), "gfx/shell/continuum/games/%s%s.png", folder, suffix );

	if( !EngFuncs::FileExists( path, false ))
		return false;

	pic.Load( path );
	return pic.IsValid();
}

bool GameArt( const char *folder, CImage &pic )
{
	return LoadGamePic( folder, "", pic );
}

bool GameBackdrop( const char *folder, CImage &pic )
{
	return LoadGamePic( folder, "_bd", pic );
}

/*
====================
small shared textures
====================
*/
static CImage &SharedPic( CImage &pic, const char *path )
{
	if( !pic.IsValid( ))
		pic.Load( path );
	return pic;
}

CImage &PillPic( void )
{
	static CImage pic;
	return SharedPic( pic, "gfx/shell/continuum/pill.png" );
}

CImage &DotPic( void )
{
	static CImage pic;
	return SharedPic( pic, "gfx/shell/continuum/dot.png" );
}

CImage &ChipCurrentPic( void )
{
	static CImage pic;
	return SharedPic( pic, "gfx/shell/continuum/chip_current.png" );
}

/*
====================
CContButton
====================
*/
CContButton::CContButton() : BaseClass(),
	szHint( NULL ), szValue( NULL ), szBadge( NULL ),
	szCard( NULL ), szCardTitle( NULL ), bCaution( false ), bValueArrows( true )
{
	eTextAlignment = QM_LEFT;
	SetSize( 400, 56 );
}

void CContButton::SetScrolledRect( int x, int y, int w, int h )
{
	// keep logical pos non-negative: stock CalcPosition (which the framework
	// may run between our layout passes) bottom-anchors negative coordinates
	pos = Point( x, Q_max( y, 0 ));
	size = Size( w, h );

	m_scPos = Point( x * uiStatic.scaleX, y * uiStatic.scaleY );
	m_scSize = Size( w * uiStatic.scaleX, h * uiStatic.scaleY );
	m_scChSize = charSize * uiStatic.scaleY;

	if( m_pParent && !IsAbsolutePositioned( ))
		m_scPos += m_pParent->GetPositionOffset();
}

float CContButton::FocusT()
{
	if( !IsCurrentSelected( ))
		return 0.0f;

	return EaseOutCubic(( uiStatic.realTime - m_iLastFocusTime ) / 180.0f );
}

bool CContButton::KeyDown( int key )
{
	if( UI::Key::IsEnter( key ) || ( UI::Key::IsMouse( key ) && UI_CursorInRect( m_scPos, m_scSize )))
	{
		m_bPressed = true;
		_Event( QM_PRESSED );
		return true;
	}
	return false;
}

bool CContButton::KeyUp( int key )
{
	if( UI::Key::IsEnter( key ) || UI::Key::IsMouse( key ))
	{
		// the holder broadcasts mouse-ups to every visible item; only the
		// row that actually took the press may fire (otherwise one click
		// triggers every row's action and stacks one launch sound per row)
		if( !m_bPressed )
			return false;

		m_bPressed = false;
		if( !FBitSet( iFlags, QMF_GRAYED ))
		{
			PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
			_Event( QM_RELEASED );
		}
		return true;
	}
	return false;
}

void CContButton::Draw()
{
	if( RowClipped( m_scPos.y, m_scSize.h ))
		return;

	const float t = FocusT();
	const bool grayed = FBitSet( iFlags, QMF_GRAYED );
	const unsigned int accent = bCaution ? clrCaution : clrAccent;

	const int x = m_scPos.x;
	const int y = m_scPos.y;
	const int w = m_scSize.w;
	const int h = m_scSize.h;
	const int slide = t * 12 * uiStatic.scaleX;

	if( t > 0.0f )
	{
		// focus bar: soft fill + accent left edge
		unsigned int fill = grayed ? 0x14FFFFFF : clrAccentSoft;
		UI_FillRect( x, y, w, h, fill );
		UI_FillRect( x, y, 3 * uiStatic.scaleX, h, grayed ? clrInkFaint : accent );
	}

	const int labelH = 17 * uiStatic.scaleY;
	const int hintH = 12 * uiStatic.scaleY;
	const int padX = x + 22 * uiStatic.scaleX + slide;

	unsigned int labelColor = grayed ? clrInkFaint : ( t > 0.0f ? clrInk : clrInkDim );
	int labelY = y + ( h - labelH ) / 2;

	// reserve space for the hint while focused; keep clear daylight between
	// the label's descenders and the hint
	if( szHint && t > 0.0f )
		labelY = y + h / 2 - labelH - 5 * uiStatic.scaleY;

	// returns the rightmost x reached
	int labelEnd = UI_DrawString( fontItem, padX, labelY, w, labelH * 1.45f,
		szName, labelColor, labelH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );

	if( szBadge )
	{
		const int bh = 11 * uiStatic.scaleY;
		const int bx = labelEnd + 12 * uiStatic.scaleX;
		const int bw = g_FontMgr->GetTextWideScaled( fontSmall, szBadge, bh ) + 12 * uiStatic.scaleX;
		const int by = labelY + ( labelH - bh ) / 2;
		UI_DrawRectangleExt( bx, by - 3 * uiStatic.scaleY, bw, bh + 6 * uiStatic.scaleY, 0x7DE8B53F, 1 );
		UI_DrawString( fontSmall, bx + 6 * uiStatic.scaleX, by, bw, bh * 1.45f,
			szBadge, clrCaution, bh, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	}

	if( szHint && t > 0.0f )
	{
		// fade the hint in with focus
		unsigned int hintColor = ( clrInkFaint & 0x00FFFFFF ) | ((unsigned int)( t * 255.0f ) << 24 );
		UI_DrawString( fontHint, padX, y + h / 2 + 6 * uiStatic.scaleY, w - 30 * uiStatic.scaleX, hintH * 1.45f,
			szHint, hintColor, hintH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );
	}

	if( szValue )
	{
		const int vh = 16 * uiStatic.scaleY;
		const int arrowPad = 26 * uiStatic.scaleX;
		const int vw = g_FontMgr->GetTextWideScaled( fontBody, szValue, vh );
		const int vx = x + w - vw - 30 * uiStatic.scaleX - arrowPad;
		const int vy = y + ( h - vh ) / 2;

		UI_DrawString( fontBody, vx, vy, vw + 4, vh * 1.45f, szValue,
			grayed ? clrInkFaint : clrInk, vh, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

		if( t > 0.0f && !grayed && bValueArrows )
		{
			UI_DrawString( fontBody, vx - arrowPad, vy, arrowPad, vh * 1.45f, "<",
				accent, vh, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
			UI_DrawString( fontBody, vx + vw + 10 * uiStatic.scaleX, vy, arrowPad, vh * 1.45f, ">",
				accent, vh, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
		}
	}
}
}
