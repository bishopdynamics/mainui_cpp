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
#include "YesNoMessageBox.h"
#include "keydefs.h"

using namespace Cont;

#define ROW_W      760
#define ROW_H      52
#define HEADER_H   40
#define CONTENT_TOP 208
#define CONTENT_BOTTOM ( 768 - LEGEND_H - 22 )

// Is a high-definition content pack present for the active game? HL and its
// expansions ship models/sprites/sounds in a sibling <gamedir>_hd folder that
// the engine only mounts when fs_mount_hd is set. The folder isn't on the
// search path until then, so probe it through the read-only base path (./)
// for a few models common to every official HD pack.
static bool HdContentPresent( void )
{
	char gamedir[64] = "valve";
	EngFuncs::GetGameDir( gamedir );

	static const char *markers[] =
	{
		"models/gman.mdl", "models/barney.mdl", "models/agrunt.mdl"
	};

	for( size_t i = 0; i < V_ARRAYSIZE( markers ); i++ )
	{
		char path[160];
		snprintf( path, sizeof( path ), "%s_hd/%s", gamedir, markers[i] );
		if( EngFuncs::FileExists( path, false ))
			return true;
	}
	return false;
}

/*
====================
screen-specific row widgets (the shared ones live in Continuum.h)
====================
*/

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
class CContGlyphPreviewRow : public CContButton
{
public:
	CContGlyphPreviewRow() { iFlags |= QMF_INACTIVE; }

	void Draw() override
	{
		if( RowClipped( m_scPos.y, m_scSize.h ))
			return;

		const int labelH = 16 * uiStatic.scaleY;
		UI_DrawString( fontBody, m_scPos.x + 22 * uiStatic.scaleX, m_scPos.y + ( m_scSize.h - labelH ) / 2,
			m_scSize.w, labelH * 1.45f, "Preview", clrInkFaint, labelH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

		const int gh = 32 * uiStatic.scaleY;
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

public:
	void FocusUiToggle(); // land on the Interface tab with Classic Menu focused

private:
	enum ETab { TAB_VIDEO = 0, TAB_AUDIO, TAB_CONTROLS, TAB_INTERFACE, TAB_ADVANCED, TAB_COUNT };

	void SetTab( int tab );
	void ApplyLayout();
	void ResetTabDefaults();
	void AddRow( int tab, CContButton &item, int logicalH );

	struct entry_t
	{
		CContButton *item;
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
	CMenuBaseItem *m_pLastFocus = NULL;

	// tab bar hitboxes, rebuilt every Draw for mouse support
	struct { int x, y, w, h; } m_TabRects[TAB_COUNT] = {};

	// video
	CContDropdownRow resolution;
	CContDropdownRow windowMode;
	CContSliderRow fov;
	CContToggleRow vsync;
	CContSliderRow gamma;
	CContSliderRow brightness;

	// pending-video apply/confirm state
	void ReloadVideoRows();
	bool HasPendingVideo() const;
	void ApplyVideo();
	void RevertVideo();
	void KeepVideo();
	CMenuYesNoMessageBox confirmDialog;
	char szConfirmMsg[96];
	float flConfirmDeadline = 0;
	bool bAwaitConfirm = false;
	int iRevertMode = 0, iRevertFS = 0;

	// audio
	CContSliderRow volMaster, volMusic, volSuit;

	// controls
	CContHeader hdrMouse, hdrPad, hdrGyro;
	CContSliderRow sensitivity;
	CContInvertRow invertLook;
	CContToggleRow rawInput, mouseFilter, autoAim;
	CContButton keyBindings;
	CContToggleRow padEnable;
	CContMultiSliderRow padLook;
	CContInvertRow padInvert;
	CContMultiSliderRow padDeadzone;
	CContToggleRow gyroEnable;
	CContMultiSliderRow gyroSens;
	CContButton gyroCalibrate;
	CContButton gamepadOptions;

	// interface
	CContSpinRow glyphStyle;
	CContGlyphPreviewRow glyphPreview;
	CContToggleRow showFps, showMapName, crosshairToggle, classicUi, hdModels;

	// advanced
	CContHeader hdrStream, hdrTex, hdrLight, hdrFx, hdrPerf, hdrConsole;
	CContToggleRow levelStreaming, enableCheats;
	CContSpinRow aniso, texFilter, lmFilter;
	CContToggleRow detailTex, overbright, dynLights, shadows, lightExt, ripple, litWater, fovAdjust, conEnable;
	CContSliderRow ambient, lodBias, conFontSize;
	CContSpinRow decals, fpsMax, renderScale, conFont;
	CContMsaaRow msaa;
};

void CMenuContConfig::AddRow( int tab, CContButton &item, int logicalH )
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
	resolution.szHint = "Press X to apply";

	static const char *wmLabels[] = { "Windowed", "Fullscreen", "Borderless" };
	windowMode.SetNameAndStatus( "Display Mode", NULL );
	windowMode.szHint = "Press X to apply";
	windowMode.SetOptions( wmLabels, 3 );

	confirmDialog.SetPositiveButton( "Keep", PC_OK );
	confirmDialog.SetNegativeButton( "Revert", PC_CANCEL );
	confirmDialog.onPositive = VoidCb( &CMenuContConfig::KeepVideo );
	confirmDialog.onNegative = VoidCb( &CMenuContConfig::RevertVideo );
	confirmDialog.Link( this );

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
	hdrMouse.SetNameAndStatus( "INPUT", NULL );
	AddRow( TAB_CONTROLS, hdrMouse, HEADER_H );

	keyBindings.SetNameAndStatus( "Input Bindings", NULL );
	keyBindings.szHint = "Rebind every action - keyboard, mouse and gamepad";
	keyBindings.onReleased = UI_ContBindings_Menu;
	AddRow( TAB_CONTROLS, keyBindings, ROW_H );

	sensitivity.SetNameAndStatus( "Mouse Sensitivity", NULL );
	sensitivity.Setup( "sensitivity", 0.1f, 10.0f, 0.1f, 3.0f, 1 );
	AddRow( TAB_CONTROLS, sensitivity, ROW_H );

	invertLook.SetNameAndStatus( "Invert Mouse", NULL );
	invertLook.szHint = "Pull down to look up";
	invertLook.SetupSign( "m_pitch" );
	AddRow( TAB_CONTROLS, invertLook, ROW_H );

	rawInput.SetNameAndStatus( "Raw Input", NULL );
	rawInput.szHint = "Mouse input bypasses desktop acceleration";
	rawInput.Setup( "m_rawinput", 1 );
	AddRow( TAB_CONTROLS, rawInput, ROW_H );

	mouseFilter.SetNameAndStatus( "Mouse Smoothing", NULL );
	mouseFilter.szHint = "Averages mouse movement across frames";
	mouseFilter.Setup( "m_filter", 0 );
	AddRow( TAB_CONTROLS, mouseFilter, ROW_H );

	autoAim.SetNameAndStatus( "Auto-Aim", NULL );
	autoAim.szHint = "The original console-style aim assist";
	autoAim.Setup( "sv_aim", 0 );
	AddRow( TAB_CONTROLS, autoAim, ROW_H );

	hdrPad.SetNameAndStatus( "GAMEPAD", NULL );
	AddRow( TAB_CONTROLS, hdrPad, HEADER_H );

	padEnable.SetNameAndStatus( "Enable Gamepad", NULL );
	padEnable.Setup( "joy_enable", 1 );
	AddRow( TAB_CONTROLS, padEnable, ROW_H );

	static const char *lookCvars[] = { "joy_pitch", "joy_yaw" };
	padLook.SetNameAndStatus( "Look Sensitivity", NULL );
	padLook.szHint = "Right stick look speed";
	padLook.SetupMulti( lookCvars, 2, 20, 300, 10, 100, 0 );
	AddRow( TAB_CONTROLS, padLook, ROW_H );

	padInvert.SetNameAndStatus( "Invert Stick Y", NULL );
	padInvert.szHint = "Pull down to look up";
	padInvert.SetupSign( "joy_pitch" );
	AddRow( TAB_CONTROLS, padInvert, ROW_H );

	static const char *dzCvars[] = { "joy_side_deadzone", "joy_forward_deadzone", "joy_pitch_deadzone", "joy_yaw_deadzone" };
	padDeadzone.SetNameAndStatus( "Stick Deadzone", NULL );
	padDeadzone.szHint = "Raise if the view drifts on its own";
	padDeadzone.SetupMulti( dzCvars, 4, 0, 16384, 512, 8192, 0 );
	AddRow( TAB_CONTROLS, padDeadzone, ROW_H );

	gamepadOptions.SetNameAndStatus( "Advanced Gamepad Options", NULL );
	gamepadOptions.szHint = "Axis remapping, move sensitivity, on-screen keyboard";
	gamepadOptions.onReleased = UI_ContGamepadAxes_Menu;
	AddRow( TAB_CONTROLS, gamepadOptions, ROW_H );

	hdrGyro.SetNameAndStatus( "GYRO", NULL );
	AddRow( TAB_CONTROLS, hdrGyro, HEADER_H );

	gyroEnable.SetNameAndStatus( "Gyro Aim", NULL );
	gyroEnable.szHint = "Fine-tune your aim by tilting the controller";
	gyroEnable.Setup( "joy_gyro_enable", 0 );
	AddRow( TAB_CONTROLS, gyroEnable, ROW_H );

	static const char *gyroCvars[] = { "joy_gyro_pitch", "joy_gyro_yaw" };
	gyroSens.SetNameAndStatus( "Gyro Sensitivity", NULL );
	gyroSens.SetupMulti( gyroCvars, 2, 0.1f, 4.0f, 0.1f, 1.0f, 1 );
	AddRow( TAB_CONTROLS, gyroSens, ROW_H );

	gyroCalibrate.SetNameAndStatus( "Calibrate Gyroscope", NULL );
	gyroCalibrate.szHint = "Put the controller on a flat surface first";
	gyroCalibrate.onReleased.SetCommand( false, "joy_calibrate_gyro\n" );
	AddRow( TAB_CONTROLS, gyroCalibrate, ROW_H );

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

	crosshairToggle.SetNameAndStatus( "Crosshair", NULL );
	crosshairToggle.Setup( "crosshair", 1 );

	classicUi.SetNameAndStatus( "Classic Menu", NULL );
	classicUi.szHint = "Switch to the original menu style";
	classicUi.Setup( "ui_classic", 0 );
	// the row writes the cvar before QM_CHANGED fires; rebuild the menu so
	// the switch happens right now, not on the next menu open
	SET_EVENT_MULTI( classicUi.onChanged,
	{
		(void)pSelf; (void)pExtra;
		g_bUiFamilySwitch = true;
		UI_CloseMenu();
		UI_SetActiveMenu( true );
	});

	// HD model pack: only meaningful when the game ships a <gamedir>_hd folder
	hdModels.SetNameAndStatus( "HD Models", NULL );
	hdModels.szHint = "Use the high-definition model & sprite pack";
	hdModels.Setup( "fs_mount_hd", 0 );
	// fs_mount_hd only changes which files resolve after a filesystem rescan;
	// the row writes the cvar, then we remount so it applies without a restart
	SET_EVENT_MULTI( hdModels.onChanged,
	{
		(void)pSelf; (void)pExtra;
		EngFuncs::ClientCmd( false, "fs_rescan\n" );
	});

	AddRow( TAB_INTERFACE, glyphStyle, ROW_H );
	AddRow( TAB_INTERFACE, glyphPreview, ROW_H );
	AddRow( TAB_INTERFACE, crosshairToggle, ROW_H );
	AddRow( TAB_INTERFACE, showFps, ROW_H );
	AddRow( TAB_INTERFACE, showMapName, ROW_H );
	if( HdContentPresent( ))
		AddRow( TAB_INTERFACE, hdModels, ROW_H );
	AddRow( TAB_INTERFACE, classicUi, ROW_H );

	// ---- advanced ----
	hdrStream.SetNameAndStatus( "STREAMING", NULL );
	AddRow( TAB_ADVANCED, hdrStream, HEADER_H );

	levelStreaming.SetNameAndStatus( "Level Streaming", NULL );
	levelStreaming.szHint = "Seamless transitions, no loading screens";
	levelStreaming.szCardTitle = "LEVEL STREAMING";
	levelStreaming.szCard =
		"The whole campaign is preloaded into memory at launch (about half "
		"a gigabyte for Half-Life) and level transitions happen invisibly - "
		"no loading screens, sounds and music carry across.\n\n"
		"Turn it off to get the classic loading-screen experience and "
		"reclaim the memory on next launch. Some people like to suffer.";
	levelStreaming.Setup( "host_level_streaming", 1 );
	AddRow( TAB_ADVANCED, levelStreaming, ROW_H );

	enableCheats.SetNameAndStatus( "Enable Cheats", NULL );
	enableCheats.szHint = "Adds a Cheats page to the in-game menu";
	enableCheats.bCaution = true;
	enableCheats.szCardTitle = "ENABLE CHEATS";
	enableCheats.szCard =
		"Sets sv_cheats and adds a Cheats page to the in-game menu (god mode, "
		"noclip, give weapons, and so on).\n\n"
		"Toggle cheats like god mode are re-applied automatically after every "
		"seamless level change, so they don't silently switch off mid-game.";
	enableCheats.Setup( "sv_cheats", 0 );
	AddRow( TAB_ADVANCED, enableCheats, ROW_H );

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

	hdrConsole.SetNameAndStatus( "CONSOLE", NULL );
	AddRow( TAB_ADVANCED, hdrConsole, HEADER_H );

	conEnable.SetNameAndStatus( "Enable Console", NULL );
	conEnable.szHint = "Open with the tilde key while playing";
	conEnable.Setup( "con_enable", 0 );
	AddRow( TAB_ADVANCED, conEnable, ROW_H );

	static const char *conFontLabels[] = { "Classic", "Modern" };
	static const float conFontValues[] = { 0, 1 };
	conFont.SetNameAndStatus( "Console Font", NULL );
	conFont.szHint = "Modern renders gfx/fonts/console.ttf - swap that file for any font you like";
	conFont.Setup( "con_ttffont", conFontLabels, conFontValues, 2, 1 );
	AddRow( TAB_ADVANCED, conFont, ROW_H );

	conFontSize.SetNameAndStatus( "Console Font Size", NULL );
	conFontSize.szHint = "Applies immediately";
	conFontSize.Setup( "con_fontscale", 1.0f, 2.5f, 0.1f, 1.0f, 1 );
	AddRow( TAB_ADVANCED, conFontSize, ROW_H );

	SetTab( TAB_VIDEO );
}

void CMenuContConfig::_VidInit()
{
	VidInitFonts();
	ReloadVideoRows();
	ApplyLayout();
}

/*
====================
pending video changes: nothing applies until X, then the user has 15 s to
confirm before everything reverts (a bad mode shouldn't strand them)
====================
*/
void CMenuContConfig::ReloadVideoRows()
{
	const char *modes[CContDropdownRow::MAX_OPTIONS];
	int count = 0;

	for( int i = 0; i < (int)CContDropdownRow::MAX_OPTIONS; i++ )
	{
		const char *mode = EngFuncs::GetModeString( i );
		if( !mode ) break;
		modes[count++] = mode;
	}

	// the engine enumerates high -> low; present low on the left, high on
	// the right (option i maps to engine mode count-1-i)
	for( int i = 0; i < count / 2; i++ )
	{
		const char *t = modes[i];
		modes[i] = modes[count - 1 - i];
		modes[count - 1 - i] = t;
	}

	resolution.SetOptions( modes, count );

	// vid_mode can be stale; trust the actual window size when it's in the list
	char current[32];
	snprintf( current, sizeof( current ), "%ix%i",
		(int)EngFuncs::GetCvarFloat( "width" ), (int)EngFuncs::GetCvarFloat( "height" ));

	int applied = count - 1 - (int)EngFuncs::GetCvarFloat( "vid_mode" );
	for( int i = 0; i < count; i++ )
	{
		if( !stricmp( modes[i], current ))
		{
			applied = i;
			break;
		}
	}

	resolution.SetApplied( applied );
	windowMode.SetApplied( bound( 0, (int)EngFuncs::GetCvarFloat( "fullscreen" ), 2 ));
}

bool CMenuContConfig::HasPendingVideo() const
{
	return resolution.HasPending() || windowMode.HasPending();
}

void CMenuContConfig::ApplyVideo()
{
	iRevertMode = resolution.iApplied;
	iRevertFS = windowMode.iApplied;

	if( windowMode.HasPending( ))
		EngFuncs::CvarSetValue( "fullscreen", windowMode.iPending );
	if( resolution.HasPending( ))
		EngFuncs::ClientCmdF( true, "vid_setmode %i\n", resolution.nCount - 1 - resolution.iPending );

	resolution.AcceptPending();
	windowMode.AcceptPending();

	bAwaitConfirm = true;
	flConfirmDeadline = gpGlobals->time + 15.0f;

	Q_strncpy( szConfirmMsg, "Keep these video settings?", sizeof( szConfirmMsg ));
	confirmDialog.SetMessage( szConfirmMsg );
	confirmDialog.Show();
}

void CMenuContConfig::KeepVideo()
{
	bAwaitConfirm = false;
}

void CMenuContConfig::RevertVideo()
{
	if( !bAwaitConfirm )
		return;

	bAwaitConfirm = false;

	EngFuncs::CvarSetValue( "fullscreen", iRevertFS );
	EngFuncs::ClientCmdF( true, "vid_setmode %i\n", resolution.nCount - 1 - iRevertMode );

	resolution.SetApplied( iRevertMode );
	windowMode.SetApplied( iRevertFS );
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

	// gyro rows only make sense when the pad reports one
	if( m_iTab == TAB_CONTROLS )
	{
		const bool haveGyro = EngFuncs::GetCvarFloat( "joy_have_gyro" ) != 0.0f;
		gyroEnable.SetGrayed( !haveGyro );
		gyroSens.SetGrayed( !haveGyro );
		gyroCalibrate.SetGrayed( !haveGyro );
		gyroEnable.szHint = haveGyro ? "Fine-tune your aim by tilting the controller"
			: "No gyroscope detected on this controller";
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
		e.item->SetScrolledRect( MARGIN, CONTENT_TOP + e.baseY - (int)m_flScroll, ROW_W, e.height );
	}
}

void CMenuContConfig::Think()
{
	// applied-but-unconfirmed video settings revert when the clock runs out
	if( bAwaitConfirm )
	{
		const int left = (int)( flConfirmDeadline - gpGlobals->time );

		if( left < 0 )
		{
			confirmDialog.Hide();
			RevertVideo();
		}
		else
		{
			snprintf( szConfirmMsg, sizeof( szConfirmMsg ),
				"Keep these video settings? Reverting in %i s", left + 1 );
			confirmDialog.SetMessage( szConfirmMsg );
		}
	}

	// keep the focused row in view, but only react when focus MOVES so the
	// mouse wheel can scroll freely in between
	CMenuBaseItem *focus = ItemAtCursor();
	if( focus && focus != m_pLastFocus )
	{
		m_pLastFocus = focus;
		for( int i = 0; i < m_nEntries; i++ )
		{
			entry_t &e = m_Entries[i];
			if( e.item != focus || e.tab != m_iTab )
				continue;

			const int view = CONTENT_BOTTOM - CONTENT_TOP;
			if( e.baseY - m_flScrollTarget < 6 )
				m_flScrollTarget = Q_max( 0, e.baseY - 6 );
			else if( e.baseY + e.height - m_flScrollTarget > view - 6 )
				m_flScrollTarget = e.baseY + e.height - view + 6;
			break;
		}
	}

	const float dt = gpGlobals->frametime;
	m_flScroll += ( m_flScrollTarget - m_flScroll ) * bound( 0.0f, dt * 14.0f, 1.0f );

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
	// an open dropdown owns Esc (closes the list, not the screen); the base
	// window would hide on Esc before the row ever saw the key
	if( UI::Key::IsEscape( key ))
	{
		if( resolution.bOpen )
			return resolution.KeyDown( key );
		if( windowMode.bOpen )
			return windowMode.KeyDown( key );
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

	// mouse wheel scrolls the row viewport directly
	if( key == K_MWHEELUP || key == K_MWHEELDOWN )
	{
		const int view = CONTENT_BOTTOM - CONTENT_TOP;
		const float maxScroll = Q_max( 0, m_iTabHeight[m_iTab] - view );

		m_flScrollTarget = bound( 0.0f, m_flScrollTarget + ( key == K_MWHEELDOWN ? 90.0f : -90.0f ), maxScroll );
		return true;
	}

	// clicking a tab name switches to it; legend entries act as buttons
	if( key == K_MOUSE1 )
	{
		const int legendKey = LegendClickKey();
		if( legendKey )
			return KeyDown( legendKey );

		for( int i = 0; i < TAB_COUNT; i++ )
		{
			if( UI_CursorInRect( m_TabRects[i].x, m_TabRects[i].y, m_TabRects[i].w, m_TabRects[i].h ))
			{
				if( i != m_iTab )
				{
					SetTab( i );
					EngFuncs::PlayLocalSound( uiStatic.sounds[SND_MOVE] );
				}
				return true;
			}
		}
	}

	if( key == K_X_BUTTON || key == 'x' )
	{
		// on the video tab X applies pending resolution/display changes;
		// everywhere else (or with nothing pending) it restores defaults
		if( m_iTab == TAB_VIDEO && HasPendingVideo( ))
			ApplyVideo();
		else
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

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"CONFIGURATION", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		"SHARED BY ALL GAMES", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// tab bar
	static const char *tabNames[TAB_COUNT] = { "VIDEO", "AUDIO", "CONTROLS", "INTERFACE", "ADVANCED" };
	const int tabH = 15 * uiStatic.scaleY;
	const int tabY = 160 * uiStatic.scaleY;
	int x = tx;

	x += DrawGlyph( GLYPH_LB, x, tabY - 4 * uiStatic.scaleY, tabH * 1.6f ) + 18 * uiStatic.scaleX;

	for( int i = 0; i < TAB_COUNT; i++ )
	{
		const bool active = ( i == m_iTab );
		const int wide = g_FontMgr->GetTextWideScaled( fontSmall, tabNames[i], tabH );

		// generous hitbox for mouse users
		m_TabRects[i].x = x - 10 * uiStatic.scaleX;
		m_TabRects[i].y = tabY - 10 * uiStatic.scaleY;
		m_TabRects[i].w = wide + 24 * uiStatic.scaleX;
		m_TabRects[i].h = tabH + 24 * uiStatic.scaleY;

		UI_DrawString( fontSmall, x, tabY, wide + 4, tabH * 1.45f, tabNames[i],
			active ? clrInk : clrInkFaint, tabH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

		if( active )
			UI_FillRect( x, tabY + tabH + 8 * uiStatic.scaleY, wide, 3 * uiStatic.scaleY, clrAccent );

		x += wide + 34 * uiStatic.scaleX;
	}

	DrawGlyph( GLYPH_RB, x, tabY - 4 * uiStatic.scaleY, tabH * 1.6f );

	// rows, clipped to the content viewport (yOffset matters when the
	// screen is narrower than 4:3 and the menu is letterboxed)
	const int clipTop = ( CONTENT_TOP + uiStatic.yOffset ) * uiStatic.scaleY;
	const int clipBottom = clipTop + ( CONTENT_BOTTOM - CONTENT_TOP ) * uiStatic.scaleY;
	EngFuncs::PIC_EnableScissor( 0, clipTop, ScreenWidth, clipBottom - clipTop );
	SetRowClip( clipTop, clipBottom );
	CMenuFramework::Draw();
	SetRowClip( 0, 0 );
	EngFuncs::PIC_DisableScissor();

	// open dropdown overlays everything, outside the scissor
	resolution.DrawPopup();
	windowMode.DrawPopup();

	// focused rows with an explainer get a side card (when there's room
	// right of the rows; on 4:3 there isn't)
	CMenuBaseItem *focus = ItemAtCursor();
	const int cardX = MARGIN + ROW_W + 24;
	const int cardW = uiStatic.width - MARGIN - cardX;
	if( focus && cardW >= 220 )
	{
		CContButton *row = static_cast<CContButton *>( focus );
		// every focusable row on this screen derives from CContButton
		if( row->szCard )
		{
			const int cx = cardX * uiStatic.scaleX;
			const int cy = ( CONTENT_TOP + uiStatic.yOffset ) * uiStatic.scaleY;
			const int cw = cardW * uiStatic.scaleX;
			const int chH = 300 * uiStatic.scaleY;

			UI_FillRect( cx, cy, cw, chH, 0xC0101218 );
			UI_DrawRectangleExt( cx, cy, cw, chH, 0x28FFFFFF, 1 );

			const int pad = 22 * uiStatic.scaleX;
			const int titleHh = 12 * uiStatic.scaleY;
			int yy = cy + 20 * uiStatic.scaleY;

			if( row->szCardTitle )
			{
				UI_DrawString( fontSmall, cx + pad, yy, cw - pad * 2, titleHh * 1.45f,
					row->szCardTitle, clrAccent, titleHh, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
				yy += titleHh + 16 * uiStatic.scaleY;
			}

			const int bodyH = 14 * uiStatic.scaleY;
			DrawWrappedText( fontHint, cx + pad, yy, cw - pad * 2, bodyH, row->szCard, clrInkDim );
		}
	}

	if( m_iTab == TAB_VIDEO && HasPendingVideo( ))
	{
		static const LegendEntry legend[] =
		{
			{ GLYPH_A, GLYPH_COUNT, "Change" },
			{ GLYPH_B, GLYPH_COUNT, "Back" },
			{ GLYPH_LB, GLYPH_RB, "Section" },
			{ GLYPH_X, GLYPH_COUNT, "Apply" },
		};
		DrawLegend( legend, V_ARRAYSIZE( legend ));
	}
	else
	{
		static const LegendEntry legend[] =
		{
			{ GLYPH_A, GLYPH_COUNT, "Change" },
			{ GLYPH_B, GLYPH_COUNT, "Back" },
			{ GLYPH_LB, GLYPH_RB, "Section" },
			{ GLYPH_X, GLYPH_COUNT, "Restore Defaults" },
		};
		DrawLegend( legend, V_ARRAYSIZE( legend ));
	}
}

void CMenuContConfig::FocusUiToggle()
{
	SetTab( TAB_INTERFACE );

	for( int i = 0; i < m_pItems.Count(); i++ )
	{
		if( m_pItems[i] == &classicUi )
		{
			SetCursor( i );
			break;
		}
	}
}

ADD_MENU( menu_continuum_config, CMenuContConfig, UI_ContConfig_Menu );

/*
================
UI_ContConfig_FocusUiToggle

family-switch landing spot: the Interface tab with Classic Menu focused
================
*/
void UI_ContConfig_FocusUiToggle( void )
{
	UI_ContConfig_Menu();
	menu_continuum_config->FocusUiToggle();
}

/*
====================
advanced gamepad options: hardware axis mapping, move sensitivity, OSK
====================
*/
static const char *g_szAxisLabels[] =
{
	"Side", "Forward", "Pitch", "Yaw", "Right Trigger", "Left Trigger", "Not Bound"
};
static const char g_szAxisChars[] = "sfpyrl0";

// one hardware axis slot of joy_axis_binding (default "sfpyrl")
class CContAxisRow : public CContSpinRow
{
public:
	void SetupAxis( int idx )
	{
		iAxis = idx;
		szCvar = "joy_axis_binding"; // written by Write() below, char-wise
		szLabels = g_szAxisLabels;
		nCount = 7;
		iDefault = idx; // default map is the identity: "sfpyrl"
	}

	void Reload() override
	{
		const char *b = EngFuncs::GetCvarString( "joy_axis_binding" );

		iIndex = 6; // not bound
		if( iAxis < (int)strlen( b ))
		{
			for( int i = 0; i < 6; i++ )
			{
				if( b[iAxis] == g_szAxisChars[i] )
				{
					iIndex = i;
					break;
				}
			}
		}
		szValue = szLabels[iIndex];
	}

	void Write() override
	{
		char b[8];
		Q_strncpy( b, EngFuncs::GetCvarString( "joy_axis_binding" ), sizeof( b ));

		int len = strlen( b );
		while( len < 6 )
			b[len++] = '0';
		b[6] = 0;

		b[iAxis] = g_szAxisChars[iIndex];
		EngFuncs::CvarSetString( "joy_axis_binding", b );
	}

	int iAxis;
};

class CMenuContGamepadAxes : public CMenuFramework
{
public:
	CMenuContGamepadAxes() : CMenuFramework( "CMenuContGamepadAxes" ) { }

	bool KeyDown( int key ) override;
	void Draw() override;
	void Hide() override;

private:
	void _Init() override;
	void _VidInit() override;

	CContAxisRow axis[6];
	CContMultiSliderRow moveSide, moveForward;
	CContToggleRow osk;
};

void CMenuContGamepadAxes::_Init()
{
	static const char *sideCvar[] = { "joy_side" };
	static const char *fwdCvar[] = { "joy_forward" };

	static char names[6][16];
	for( int i = 0; i < 6; i++ )
	{
		snprintf( names[i], sizeof( names[i] ), "Axis %i", i + 1 );
		axis[i].SetNameAndStatus( names[i], NULL );
		axis[i].szHint = "Which stick/trigger this hardware axis drives";
		axis[i].SetupAxis( i );
		AddItem( axis[i] );
	}

	moveSide.SetNameAndStatus( "Side Move Sensitivity", NULL );
	moveSide.SetupMulti( sideCvar, 1, 0.1f, 1.0f, 0.05f, 1.0f, 2 );
	AddItem( moveSide );

	moveForward.SetNameAndStatus( "Forward Move Sensitivity", NULL );
	moveForward.SetupMulti( fwdCvar, 1, 0.1f, 1.0f, 0.05f, 1.0f, 2 );
	AddItem( moveForward );

	osk.SetNameAndStatus( "On-Screen Keyboard", NULL );
	osk.szHint = "Pop up for every text field - off still offers it on (A) when a pad is connected";
	osk.Setup( "osk_enable", 0 );
	AddItem( osk );
}

void CMenuContGamepadAxes::_VidInit()
{
	VidInitFonts();

	const int itemH = 50, gap = 4;
	int y = 208;

	for( int i = 0; i < 6; i++, y += itemH + gap )
		axis[i].SetRect( MARGIN, y, ROW_W, itemH );

	moveSide.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	moveForward.SetRect( MARGIN, y, ROW_W, itemH );
	y += itemH + gap;
	osk.SetRect( MARGIN, y, ROW_W, itemH );
}

bool CMenuContGamepadAxes::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ))
	{
		Hide();
		return true;
	}

	if( key == K_MOUSE1 )
	{
		const int legendKey = LegendClickKey();
		if( legendKey )
			return KeyDown( legendKey );
	}

	if( key == K_X_BUTTON || key == 'x' )
	{
		for( int i = 0; i < 6; i++ )
			axis[i].ResetDefault();
		moveSide.ResetDefault();
		moveForward.ResetDefault();
		osk.ResetDefault();
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
		return true;
	}

	return CMenuFramework::KeyDown( key );
}

void CMenuContGamepadAxes::Hide()
{
	EngFuncs::ClientCmd( false, "host_writeconfig\n" );
	CMenuFramework::Hide();
}

void CMenuContGamepadAxes::Draw()
{
	static CImage noBackdrop;
	DrawBackdrop( noBackdrop );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"GAMEPAD", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		"AXIS MAPPING - SHARED BY ALL GAMES", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	CMenuFramework::Draw();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Change" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
		{ GLYPH_X, GLYPH_COUNT, "Restore Defaults" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ));
}

ADD_MENU( menu_continuum_gamepadaxes, CMenuContGamepadAxes, UI_ContGamepadAxes_Menu );
