/*
Config.cpp -- Continuum unified configuration: tabbed settings screen
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

using namespace Cont;

#define ROW_W      760
#define ROW_H      52
#define HEADER_H   40
#define CONTENT_TOP 208
#define CONTENT_BOTTOM ( 768 - LEGEND_H - 14 )

/*
====================
row widgets
====================
*/

// section header, skipped by the cursor
class CContHeader : public CMenuBaseItem
{
public:
	CContHeader() { iFlags |= QMF_INACTIVE; }

	void Draw() override
	{
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
		if( UI::Key::IsLeftArrow( key ) || UI::Key::IsRightArrow( key ) || UI::Key::IsEnter( key ))
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
		if( UI::Key::IsLeftArrow( key ) || UI::Key::IsRightArrow( key ))
		{
			const int dir = UI::Key::IsRightArrow( key ) ? 1 : -1;
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

// resolution spin: dynamic labels from the engine mode list, applies through
// the vid_setmode command
class CContResolutionRow : public CContSpinRow
{
public:
	enum { MAX_MODES = 64 };

	void Refresh()
	{
		nCount = 0;
		for( int i = 0; i < MAX_MODES; i++ )
		{
			const char *mode = EngFuncs::GetModeString( i );
			if( !mode ) break;
			m_szModes[nCount++] = mode;
		}
		szLabels = m_szModes;
		iIndex = bound( 0, (int)EngFuncs::GetCvarFloat( "vid_mode" ), nCount - 1 );
		iDefault = iIndex;
		if( nCount )
			szValue = szLabels[iIndex];
	}

	void Reload() override { Refresh(); }
	void Write() override { EngFuncs::ClientCmdF( true, "vid_setmode %i\n", iIndex ); }
	void ResetDefault() override { } // no meaningful "default" resolution

private:
	const char *m_szModes[MAX_MODES];
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
		return CContButton::KeyDown( key );
	}

	void Draw() override
	{
		CContButton::Draw();

		const int numH = 13 * uiStatic.scaleY;
		char num[16];
		snprintf( num, sizeof( num ), "%.*f", nDecimals, flValue );

		const int trackW = 170 * uiStatic.scaleX;
		const int numW = 44 * uiStatic.scaleX;
		const int tx = m_scPos.x + m_scSize.w - trackW - numW - 30 * uiStatic.scaleX;
		const int cy = m_scPos.y + m_scSize.h / 2;
		const float frac = ( flValue - flMin ) / ( flMax - flMin );

		UI_FillRect( tx, cy - 2 * uiStatic.scaleY, trackW, 4 * uiStatic.scaleY, 0x3CFFFFFF );
		UI_FillRect( tx, cy - 2 * uiStatic.scaleY, trackW * frac, 4 * uiStatic.scaleY, bCaution ? clrCaution : clrAccent );

		const int knob = 12 * uiStatic.scaleY;
		UI_FillRect( tx + trackW * frac - knob / 2, cy - knob / 2, knob, knob, clrInk );

		UI_DrawString( fontHint, tx + trackW + 12 * uiStatic.scaleX, cy - numH / 2 - 2, numW, numH * 1.45f,
			num, clrInkDim, numH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	}

	const char *szCvar;
	float flMin, flMax, flStep, flDefault, flValue;
	int nDecimals;
};

// invert look: UI face over the sign of m_pitch
class CContInvertRow : public CContToggleRow
{
public:
	void Reload() override { bOn = EngFuncs::GetCvarFloat( "m_pitch" ) < 0.0f; }
	void ResetDefault() override
	{
		EngFuncs::CvarSetValue( "m_pitch", fabs( EngFuncs::GetCvarFloat( "m_pitch" )));
		Reload();
	}

	bool KeyDown( int key ) override
	{
		if( UI::Key::IsLeftArrow( key ) || UI::Key::IsRightArrow( key ) || UI::Key::IsEnter( key ))
		{
			const float pitch = EngFuncs::GetCvarFloat( "m_pitch" );
			EngFuncs::CvarSetValue( "m_pitch", -pitch );
			bOn = -pitch < 0.0f;
			PlayLocalSound( uiStatic.sounds[SND_MOVE] );
			return true;
		}
		return CContButton::KeyDown( key );
	}
};

// MSAA also flips the gl_msaa master switch
class CContMsaaRow : public CContSpinRow
{
public:
	void Write() override
	{
		CContSpinRow::Write();
		EngFuncs::CvarSetValue( "gl_msaa", iIndex > 0 ? 1.0f : 0.0f );
	}
};

// inert row that previews the active glyph set
class CContGlyphPreviewRow : public CMenuBaseItem
{
public:
	CContGlyphPreviewRow() { iFlags |= QMF_INACTIVE; }

	void Draw() override
	{
		const int labelH = 16 * uiStatic.scaleY;
		UI_DrawString( fontBody, m_scPos.x + 22 * uiStatic.scaleX, m_scPos.y + ( m_scSize.h - labelH ) / 2,
			m_scSize.w, labelH * 1.45f, "Preview", clrInkFaint, labelH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

		const int gh = 26 * uiStatic.scaleY;
		int x = m_scPos.x + m_scSize.w - 30 * uiStatic.scaleX;
		const int gy = m_scPos.y + ( m_scSize.h - gh ) / 2;

		// face buttons are square in every style, so right-to-left is easy
		static const EGlyph order[] = { GLYPH_Y, GLYPH_X, GLYPH_B, GLYPH_A };
		for( size_t i = 0; i < V_ARRAYSIZE( order ); i++ )
		{
			x -= gh;
			DrawGlyph( order[i], x, gy, gh );
			x -= 8 * uiStatic.scaleX;
		}
	}
};

/*
====================
the screen
====================
*/
class CMenuContConfig : public CMenuFramework
{
public:
	CMenuContConfig() : CMenuFramework( "CMenuContConfig" ), m_iTab( 0 ), m_flScroll( 0 ), m_flScrollTarget( 0 ) { }

	bool KeyDown( int key ) override;
	void Draw() override;
	void Think() override;
	void Hide() override;

private:
	void _Init() override;
	void _VidInit() override;

	enum ETab { TAB_VIDEO = 0, TAB_AUDIO, TAB_CONTROLS, TAB_INTERFACE, TAB_ADVANCED, TAB_COUNT };

	void SetTab( int tab );
	void ApplyLayout();
	void ResetTabDefaults();
	void AddRow( int tab, CMenuBaseItem &item, int logicalH );

	struct entry_t
	{
		CMenuBaseItem *item;
		int tab;
		int baseY;     // logical y inside the tab, before scroll
		int height;
	};

	enum { MAX_ENTRIES = 64 };
	entry_t m_Entries[MAX_ENTRIES];
	int m_nEntries = 0;
	int m_iTabHeight[TAB_COUNT] = {};

	int m_iTab;
	float m_flScroll, m_flScrollTarget;

	// video
	CContResolutionRow resolution;
	CContSpinRow windowMode;
	CContSliderRow fov;
	CContToggleRow vsync;
	CContSliderRow gamma;
	CContSliderRow brightness;

	// audio
	CContSliderRow volMaster, volMusic, volSuit;

	// controls
	CContSliderRow sensitivity;
	CContInvertRow invertLook;
	CContButton keyBindings;
	CContButton gamepadOptions;

	// interface
	CContSpinRow glyphStyle;
	CContGlyphPreviewRow glyphPreview;
	CContToggleRow showFps, showMapName;

	// advanced
	CContHeader hdrTex, hdrLight, hdrFx, hdrPerf;
	CContSpinRow aniso, texFilter, lmFilter;
	CContToggleRow detailTex, overbright, dynLights, shadows, lightExt, ripple, litWater, fovAdjust;
	CContSliderRow ambient, lodBias;
	CContSpinRow decals, fpsMax, renderScale;
	CContMsaaRow msaa;
};

void CMenuContConfig::AddRow( int tab, CMenuBaseItem &item, int logicalH )
{
	if( m_nEntries >= MAX_ENTRIES )
		return;

	entry_t &e = m_Entries[m_nEntries++];
	e.item = &item;
	e.tab = tab;
	e.baseY = m_iTabHeight[tab];
	e.height = logicalH;
	m_iTabHeight[tab] += logicalH + 4;

	AddItem( item );
}

void CMenuContConfig::_Init()
{
	static const char *onOff[] = { "Off", "On" }; (void)onOff;

	// ---- video ----
	resolution.SetNameAndStatus( "Resolution", NULL );
	resolution.szHint = "Applied immediately";

	static const char *wmLabels[] = { "Windowed", "Fullscreen", "Borderless" };
	static const float wmValues[] = { 0, 1, 2 };
	windowMode.SetNameAndStatus( "Display Mode", NULL );
	windowMode.Setup( "fullscreen", wmLabels, wmValues, 3, 2 );

	fov.SetNameAndStatus( "Field of View", NULL );
	fov.Setup( "default_fov", 70, 120, 5, 90, 0 );

	vsync.SetNameAndStatus( "V-Sync", NULL );
	vsync.Setup( "gl_vsync", 1 );

	gamma.SetNameAndStatus( "Gamma", NULL );
	gamma.Setup( "gamma", 1.8f, 3.0f, 0.05f, 2.5f, 2 );

	brightness.SetNameAndStatus( "Brightness", NULL );
	brightness.Setup( "brightness", 0.0f, 3.0f, 0.1f, 0.0f, 1 );

	AddRow( TAB_VIDEO, resolution, ROW_H );
	AddRow( TAB_VIDEO, windowMode, ROW_H );
	AddRow( TAB_VIDEO, fov, ROW_H );
	AddRow( TAB_VIDEO, vsync, ROW_H );
	AddRow( TAB_VIDEO, gamma, ROW_H );
	AddRow( TAB_VIDEO, brightness, ROW_H );

	// ---- audio ----
	volMaster.SetNameAndStatus( "Master Volume", NULL );
	volMaster.Setup( "volume", 0, 1, 0.05f, 0.8f, 2 );
	volMusic.SetNameAndStatus( "Music Volume", NULL );
	volMusic.Setup( "MP3Volume", 0, 1, 0.05f, 1.0f, 2 );
	volSuit.SetNameAndStatus( "Suit Volume", NULL );
	volSuit.Setup( "suitvolume", 0, 1, 0.05f, 0.25f, 2 );

	AddRow( TAB_AUDIO, volMaster, ROW_H );
	AddRow( TAB_AUDIO, volMusic, ROW_H );
	AddRow( TAB_AUDIO, volSuit, ROW_H );

	// ---- controls ----
	sensitivity.SetNameAndStatus( "Mouse Sensitivity", NULL );
	sensitivity.Setup( "sensitivity", 0.1f, 10.0f, 0.1f, 3.0f, 1 );

	invertLook.SetNameAndStatus( "Invert Mouse", NULL );
	invertLook.szHint = "Pull down to look up";

	keyBindings.SetNameAndStatus( "Keyboard & Mouse Bindings", NULL );
	keyBindings.szHint = "Rebind every action";
	keyBindings.onReleased = UI_Controls_Menu;

	gamepadOptions.SetNameAndStatus( "Gamepad Options", NULL );
	gamepadOptions.szHint = "Sticks, sensitivity & gyro";
	gamepadOptions.onReleased = UI_GamePad_Menu;

	AddRow( TAB_CONTROLS, sensitivity, ROW_H );
	AddRow( TAB_CONTROLS, invertLook, ROW_H );
	AddRow( TAB_CONTROLS, keyBindings, ROW_H );
	AddRow( TAB_CONTROLS, gamepadOptions, ROW_H );

	// ---- interface ----
	static const char *glyphLabels[] = { "Auto", "Xbox", "PlayStation", "Switch", "Steam Deck", "Keyboard" };
	static const char *glyphValues[] = { "auto", "xbox", "ps", "switch", "deck", "kb" };
	glyphStyle.SetNameAndStatus( "Button Glyphs", NULL );
	glyphStyle.szHint = "Shown in menus and hints";
	glyphStyle.SetupString( "ui_glyph_style", glyphLabels, glyphValues, 6, 0 );

	showFps.SetNameAndStatus( "FPS Counter", NULL );
	showFps.Setup( "cl_showfps", 0 );

	showMapName.SetNameAndStatus( "Show Map Name", NULL );
	showMapName.Setup( "scr_drawmapname", 1 );

	AddRow( TAB_INTERFACE, glyphStyle, ROW_H );
	AddRow( TAB_INTERFACE, glyphPreview, ROW_H );
	AddRow( TAB_INTERFACE, showFps, ROW_H );
	AddRow( TAB_INTERFACE, showMapName, ROW_H );

	// ---- advanced ----
	hdrTex.SetNameAndStatus( "TEXTURES", NULL );
	AddRow( TAB_ADVANCED, hdrTex, HEADER_H );

	static const char *anisoLabels[] = { "Off", "2x", "4x", "8x", "16x" };
	static const float anisoValues[] = { 1, 2, 4, 8, 16 };
	aniso.SetNameAndStatus( "Anisotropic Filtering", NULL );
	aniso.szHint = "Sharper textures at glancing angles - nearly free on modern GPUs";
	aniso.Setup( "gl_anisotropy", anisoLabels, anisoValues, 5, 3 );
	AddRow( TAB_ADVANCED, aniso, ROW_H );

	static const char *filterLabels[] = { "Smooth", "Nearest" };
	static const float filterValues[] = { 0, 1 };
	texFilter.SetNameAndStatus( "Texture Filtering", NULL );
	texFilter.szHint = "Nearest = the classic 1998 software-renderer look";
	texFilter.Setup( "gl_texture_nearest", filterLabels, filterValues, 2, 0 );
	AddRow( TAB_ADVANCED, texFilter, ROW_H );

	lmFilter.SetNameAndStatus( "Lightmap Filtering", NULL );
	lmFilter.szHint = "Pairs with texture filtering for the retro look";
	lmFilter.Setup( "gl_lightmap_nearest", filterLabels, filterValues, 2, 0 );
	AddRow( TAB_ADVANCED, lmFilter, ROW_H );

	detailTex.SetNameAndStatus( "Detail Textures", NULL );
	detailTex.Setup( "r_detailtextures", 1 );
	AddRow( TAB_ADVANCED, detailTex, ROW_H );

	hdrLight.SetNameAndStatus( "LIGHTING", NULL );
	AddRow( TAB_ADVANCED, hdrLight, HEADER_H );

	overbright.SetNameAndStatus( "Overbright Lighting", NULL );
	overbright.szHint = "Matches the original GoldSrc look";
	overbright.Setup( "gl_overbright", 1 );
	AddRow( TAB_ADVANCED, overbright, ROW_H );

	dynLights.SetNameAndStatus( "Dynamic Lights", NULL );
	dynLights.szHint = "Muzzle flashes and explosions light the world";
	dynLights.Setup( "r_dynamic", 1 );
	AddRow( TAB_ADVANCED, dynLights, ROW_H );

	shadows.SetNameAndStatus( "Entity Shadows", NULL );
	shadows.szHint = "Simple shadows under players and monsters";
	shadows.Setup( "r_shadows", 0 );
	AddRow( TAB_ADVANCED, shadows, ROW_H );

	ambient.SetNameAndStatus( "Ambient Light", NULL );
	ambient.szHint = "Raise to brighten dark maps without washing out gamma";
	ambient.Setup( "r_lighting_ambient", 0, 1, 0.05f, 0.3f, 2 );
	AddRow( TAB_ADVANCED, ambient, ROW_H );

	lightExt.SetNameAndStatus( "Extended Light Sampling", NULL );
	lightExt.szHint = "Entities take light from the world and brush models";
	lightExt.Setup( "r_lighting_extended", 1 );
	AddRow( TAB_ADVANCED, lightExt, ROW_H );

	hdrFx.SetNameAndStatus( "EFFECTS", NULL );
	AddRow( TAB_ADVANCED, hdrFx, HEADER_H );

	ripple.SetNameAndStatus( "Water Ripples", NULL );
	ripple.szHint = "Software-renderer style animated water";
	ripple.Setup( "r_ripple", 0 );
	AddRow( TAB_ADVANCED, ripple, ROW_H );

	litWater.SetNameAndStatus( "Lightmapped Water", NULL );
	litWater.szHint = "Force lit water even when the map doesn't declare it";
	litWater.Setup( "gl_litwater_force", 0 );
	AddRow( TAB_ADVANCED, litWater, ROW_H );

	static const char *decalLabels[] = { "512", "1024", "4096", "8192" };
	static const float decalValues[] = { 512, 1024, 4096, 8192 };
	decals.SetNameAndStatus( "Decal Limit", NULL );
	decals.szHint = "How many bullet holes and blood splats persist";
	decals.Setup( "r_decals", decalLabels, decalValues, 4, 2 );
	AddRow( TAB_ADVANCED, decals, ROW_H );

	hdrPerf.SetNameAndStatus( "PERFORMANCE", NULL );
	AddRow( TAB_ADVANCED, hdrPerf, HEADER_H );

	static const char *fpsLabels[] = { "60", "72", "100", "120", "144", "165", "240", "Unlimited" };
	static const float fpsValues[] = { 60, 72, 100, 120, 144, 165, 240, 0 };
	fpsMax.SetNameAndStatus( "FPS Limit", NULL );
	fpsMax.szHint = "72 is the original GoldSrc pacing";
	fpsMax.Setup( "fps_max", fpsLabels, fpsValues, 8, 1 );
	AddRow( TAB_ADVANCED, fpsMax, ROW_H );

	static const char *msaaLabels[] = { "Off", "2x MSAA", "4x MSAA", "8x MSAA" };
	static const float msaaValues[] = { 0, 2, 4, 8 };
	msaa.SetNameAndStatus( "Anti-Aliasing", NULL );
	msaa.szHint = "Multisampling - applied on next launch";
	msaa.szBadge = "RESTART";
	msaa.Setup( "gl_msaa_samples", msaaLabels, msaaValues, 4, 0 );
	AddRow( TAB_ADVANCED, msaa, ROW_H );

	fovAdjust.SetNameAndStatus( "FOV Correction", NULL );
	fovAdjust.szHint = "Adjusts the field of view for wide screens";
	fovAdjust.Setup( "r_adjust_fov", 1 );
	AddRow( TAB_ADVANCED, fovAdjust, ROW_H );

	static const char *scaleLabels[] = { "1x", "2x", "3x", "4x" };
	static const float scaleValues[] = { 1, 2, 3, 4 };
	renderScale.SetNameAndStatus( "Render Scale", NULL );
	renderScale.szHint = "Renders at a fraction of the window size - retro pixels, big speedup";
	renderScale.bCaution = true;
	renderScale.Setup( "vid_scale", scaleLabels, scaleValues, 4, 0 );
	AddRow( TAB_ADVANCED, renderScale, ROW_H );

	lodBias.SetNameAndStatus( "Mipmap Sharpness", NULL );
	lodBias.szHint = "Negative = sharper but shimmery";
	lodBias.bCaution = true;
	lodBias.Setup( "gl_texture_lodbias", -2.0f, 0.0f, 0.25f, 0.0f, 2 );
	AddRow( TAB_ADVANCED, lodBias, ROW_H );

	SetTab( TAB_VIDEO );
}

void CMenuContConfig::_VidInit()
{
	VidInitFonts();
	ApplyLayout();
}

void CMenuContConfig::SetTab( int tab )
{
	m_iTab = bound( 0, tab, TAB_COUNT - 1 );
	m_flScroll = m_flScrollTarget = 0;

	int firstVisible = -1;
	for( int i = 0; i < m_nEntries; i++ )
	{
		const bool inTab = m_Entries[i].tab == m_iTab;
		m_Entries[i].item->SetVisibility( inTab );

		if( inTab && firstVisible < 0 && !FBitSet( m_Entries[i].item->iFlags, QMF_INACTIVE ))
			firstVisible = i;
	}

	ApplyLayout();

	// move cursor onto the first row of the tab
	if( firstVisible >= 0 )
	{
		for( int i = 0; i < m_pItems.Count(); i++ )
		{
			if( m_pItems[i] == m_Entries[firstVisible].item )
			{
				SetCursor( i );
				break;
			}
		}
	}
}

void CMenuContConfig::ApplyLayout()
{
	for( int i = 0; i < m_nEntries; i++ )
	{
		entry_t &e = m_Entries[i];
		e.item->SetRect( MARGIN, CONTENT_TOP + e.baseY - (int)m_flScroll, ROW_W, e.height );
		e.item->CalcPosition();
		e.item->CalcSizes();
	}
}

void CMenuContConfig::Think()
{
	// keep the focused row in view
	CMenuBaseItem *focus = ItemAtCursor();
	if( focus )
	{
		for( int i = 0; i < m_nEntries; i++ )
		{
			entry_t &e = m_Entries[i];
			if( e.item != focus || e.tab != m_iTab )
				continue;

			const int view = CONTENT_BOTTOM - CONTENT_TOP;
			if( e.baseY - m_flScrollTarget < 0 )
				m_flScrollTarget = e.baseY;
			else if( e.baseY + e.height - m_flScrollTarget > view )
				m_flScrollTarget = e.baseY + e.height - view;
			break;
		}
	}

	const float dt = gpGlobals->frametime;
	const float prev = m_flScroll;
	m_flScroll += ( m_flScrollTarget - m_flScroll ) * bound( 0.0f, dt * 14.0f, 1.0f );

	if( fabs( m_flScroll - prev ) > 0.1f )
		ApplyLayout();

	CMenuFramework::Think();
}

void CMenuContConfig::ResetTabDefaults()
{
	for( int i = 0; i < m_nEntries; i++ )
	{
		if( m_Entries[i].tab != m_iTab )
			continue;

		// headers and the preview row aren't CContRow, but they're INACTIVE
		if( FBitSet( m_Entries[i].item->iFlags, QMF_INACTIVE ))
			continue;

		static_cast<CContButton *>( m_Entries[i].item )->ResetDefault();
	}
	EngFuncs::PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
}

bool CMenuContConfig::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ))
	{
		Hide();
		return true;
	}

	if( UI::Key::IsPageUp( key ))
	{
		SetTab( m_iTab > 0 ? m_iTab - 1 : TAB_COUNT - 1 );
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
		return true;
	}

	if( UI::Key::IsPageDown( key ))
	{
		SetTab(( m_iTab + 1 ) % TAB_COUNT );
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
		return true;
	}

	if( key == K_X_BUTTON )
	{
		ResetTabDefaults();
		return true;
	}

	return CMenuFramework::KeyDown( key );
}

void CMenuContConfig::Hide()
{
	// the whole point: one config, every game
	EngFuncs::ClientCmd( false, "host_writeconfig\n" );
	CMenuFramework::Hide();
}

void CMenuContConfig::Draw()
{
	static CImage noBackdrop;
	DrawBackdrop( noBackdrop );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	(void)subH;
	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"CONFIGURATION", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// tab bar
	static const char *tabNames[TAB_COUNT] = { "VIDEO", "AUDIO", "CONTROLS", "INTERFACE", "ADVANCED" };
	const int tabH = 15 * uiStatic.scaleY;
	const int tabY = 160 * uiStatic.scaleY;
	int x = tx;

	x += DrawGlyph( GLYPH_LB, x, tabY - 2 * uiStatic.scaleY, tabH * 1.3f ) + 18 * uiStatic.scaleX;

	for( int i = 0; i < TAB_COUNT; i++ )
	{
		const bool active = ( i == m_iTab );
		const int wide = g_FontMgr->GetTextWideScaled( fontSmall, tabNames[i], tabH );

		UI_DrawString( fontSmall, x, tabY, wide + 4, tabH * 1.45f, tabNames[i],
			active ? clrInk : clrInkFaint, tabH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

		if( active )
			UI_FillRect( x, tabY + tabH + 8 * uiStatic.scaleY, wide, 3 * uiStatic.scaleY, clrAccent );

		x += wide + 34 * uiStatic.scaleX;
	}

	DrawGlyph( GLYPH_RB, x, tabY - 2 * uiStatic.scaleY, tabH * 1.3f );

	// rows, clipped to the content viewport (yOffset matters when the
	// screen is narrower than 4:3 and the menu is letterboxed)
	EngFuncs::PIC_EnableScissor( 0, ( CONTENT_TOP + uiStatic.yOffset ) * uiStatic.scaleY,
		ScreenWidth, ( CONTENT_BOTTOM - CONTENT_TOP ) * uiStatic.scaleY );
	CMenuFramework::Draw();
	EngFuncs::PIC_DisableScissor();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Change" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
		{ GLYPH_LB, GLYPH_RB, "Section" },
		{ GLYPH_X, GLYPH_COUNT, "Restore Defaults" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ), "APPLIES TO ALL GAMES" );
}

ADD_MENU( menu_continuum_config, CMenuContConfig, UI_ContConfig_Menu );
