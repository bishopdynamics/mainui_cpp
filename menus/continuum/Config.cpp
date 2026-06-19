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
	enum ETab { TAB_VIDEO = 0, TAB_AUDIO, TAB_CONTROLS, TAB_INTERFACE, TAB_GAMEPLAY, TAB_ADVANCED, TAB_COUNT };

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

	// Total rows across ALL tabs (m_nEntries is global, not per-tab). AddRow
	// silently drops anything past this cap — and a dropped row is never even
	// AddItem'd, so it's fully invisible. Keep comfortably above the real count
	// (~69 today) so late-added rows (e.g. the CONSOLE section) don't vanish.
	enum { MAX_ENTRIES = 96 };
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
	CContHeader hdrStream, hdrTex, hdrLight, hdrShadows, hdrFx, hdrPerf, hdrConsole, hdrChapters;
	CContToggleRow levelStreaming, enableCheats, showChapters;
	CContSpinRow aniso, texFilter, lmFilter;
	CContToggleRow dynLights, lightExt, ripple, litWater, fovAdjust, conEnable;
	CContScreenOverlayRow screenOverlay; // version watermark + console notify (also in screenshots)
	CContToggleRow aoWorldEnable, aoEntityEnable;
	CContButton aoCustomize;
	CContToggleRow entShadows;
	CContButton entShadowsCustomize;
	CContSliderRow ambient, lodBias, conFontSize;
	CContSpinRow decals, fpsMax, renderScale, conFont;
	CContMsaaRow msaa;

	// gameplay
	CContAlwaysRunRow alwaysRun;
	CContToggleRow flProjected, flInfinite;
	CContButton flCustomize;
};

void CMenuContConfig::AddRow( int tab, CContButton &item, int logicalH )
{
	if( m_nEntries >= MAX_ENTRIES )
	{
		// Overflow is otherwise silent: the row is never AddItem'd, so it just
		// vanishes from the menu with no error. Shout so the next person who
		// adds a setting past the cap knows to raise MAX_ENTRIES.
		Con_Printf( "^1CMenuContConfig::AddRow: MAX_ENTRIES (%d) exceeded — row dropped (tab %d). Raise MAX_ENTRIES.^7\n", (int)MAX_ENTRIES, tab );
		return;
	}

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
	showMapName.Setup( "scr_drawmapname", 0 );

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
	hdModels.szCardTitle = "HD MODELS";
	hdModels.szCard =
		"Swaps in the high-definition model and sprite pack that shipped "
		"with the later Half-Life releases - rounder character meshes and "
		"sharper skins in place of the 1998 originals.\n\n"
		"Only the official _hd content is used, so animations and "
		"silhouettes are unchanged, just more detailed. Off keeps the "
		"authentic low-poly look.";
	hdModels.Setup( "fs_mount_hd", 0 );
	// fs_mount_hd only changes which files resolve after a filesystem rescan;
	// the row writes the cvar, then fs_reapply remounts and (if a single-player
	// game is running) reloads it so the HD models swap in live without a manual
	// save/load. At the menu or in multiplayer it just rescans for the next map.
	SET_EVENT_MULTI( hdModels.onChanged,
	{
		(void)pSelf; (void)pExtra;
		EngFuncs::ClientCmd( false, "fs_reapply\n" );
	});

	AddRow( TAB_INTERFACE, glyphStyle, ROW_H );
	AddRow( TAB_INTERFACE, glyphPreview, ROW_H );
	AddRow( TAB_INTERFACE, crosshairToggle, ROW_H );
	AddRow( TAB_INTERFACE, showFps, ROW_H );
	AddRow( TAB_INTERFACE, showMapName, ROW_H );
	if( HdContentPresent( ))
		AddRow( TAB_INTERFACE, hdModels, ROW_H );
	AddRow( TAB_INTERFACE, classicUi, ROW_H );

	hdrConsole.SetNameAndStatus( "CONSOLE", NULL );
	AddRow( TAB_INTERFACE, hdrConsole, HEADER_H );

	conEnable.SetNameAndStatus( "Enable Console", NULL );
	conEnable.szHint = "Open with the tilde key while playing";
	conEnable.Setup( "con_enable", 0 );
	AddRow( TAB_INTERFACE, conEnable, ROW_H );

	static const char *conFontLabels[] = { "Classic", "Modern" };
	static const float conFontValues[] = { 0, 1 };
	conFont.SetNameAndStatus( "Console Font", NULL );
	conFont.szHint = "Modern renders gfx/fonts/console.ttf - swap that file for any font you like";
	conFont.Setup( "con_ttffont", conFontLabels, conFontValues, 2, 1 );
	AddRow( TAB_INTERFACE, conFont, ROW_H );

	conFontSize.SetNameAndStatus( "Console Font Size", NULL );
	conFontSize.szHint = "Applies immediately";
	conFontSize.Setup( "con_fontscale", 1.0f, 2.5f, 0.1f, 1.0f, 1 );
	AddRow( TAB_INTERFACE, conFontSize, ROW_H );

	screenOverlay.SetNameAndStatus( "Debugging messages", NULL );
	screenOverlay.szHint = "Engine version watermark + recent console messages (shown on screen and in screenshots)";
	AddRow( TAB_INTERFACE, screenOverlay, ROW_H );

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

	hdrChapters.SetNameAndStatus( "CHAPTERS", NULL );
	AddRow( TAB_ADVANCED, hdrChapters, HEADER_H );

	showChapters.SetNameAndStatus( "Chapter Selection", NULL );
	showChapters.szHint = "EXPERIMENTAL - jump straight into a chapter (supported games only)";
	showChapters.bCaution = true;
	showChapters.szCardTitle = "CHAPTER SELECTION";
	showChapters.szCard =
		"Adds a Chapters page for the games that ship a chapter list "
		"(Half-Life, Opposing Force, Blue Shift), so you can jump straight "
		"into any chapter.\n\n"
		"Very raw right now: some chapters can't be completed (Office "
		"Complex's elevator never opens) and you start with a generic "
		"loadout, not the one that chapter expects.";
	showChapters.Setup( "ui_chapters", 0 );
	AddRow( TAB_ADVANCED, showChapters, ROW_H );

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
	AddRow( TAB_GAMEPLAY, enableCheats, ROW_H );

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

	hdrLight.SetNameAndStatus( "LIGHTING", NULL );
	AddRow( TAB_ADVANCED, hdrLight, HEADER_H );

	dynLights.SetNameAndStatus( "Dynamic Lights", NULL );
	dynLights.szHint = "Muzzle flashes and explosions light the world";
	dynLights.Setup( "r_dynamic", 1 );
	AddRow( TAB_ADVANCED, dynLights, ROW_H );

	ambient.SetNameAndStatus( "Ambient Light", NULL );
	ambient.szHint = "Raise to brighten dark maps without washing out gamma";
	ambient.Setup( "r_lighting_ambient", 0, 1, 0.05f, 0.3f, 2 );
	AddRow( TAB_ADVANCED, ambient, ROW_H );

	lightExt.SetNameAndStatus( "Extended Light Sampling", NULL );
	lightExt.szHint = "Entities take light from the world and brush models";
	lightExt.Setup( "r_lighting_extended", 1 );
	AddRow( TAB_ADVANCED, lightExt, ROW_H );

	aoWorldEnable.SetNameAndStatus( "Additional World AO", NULL );
	aoWorldEnable.szHint = "Baked corner/recess shading on the world geometry";
	aoWorldEnable.szCardTitle = "ADDITIONAL WORLD AO";
	aoWorldEnable.szCard =
		"Ambient occlusion darkens the corners, seams and recesses of the "
		"world where light naturally struggles to reach. It is baked once "
		"per map, so it costs nothing to draw.\n\n"
		"The result is depth and contact that the original flat lightmaps "
		"never had. Subtle by design - use Customize Ambient Occlusion to "
		"push it harder.";
	aoWorldEnable.Setup( "r_ao_world", 1 );
	AddRow( TAB_ADVANCED, aoWorldEnable, ROW_H );

	aoEntityEnable.SetNameAndStatus( "Entity AO", NULL );
	aoEntityEnable.szHint = "Soft contact shadow under monsters, props and the player";
	aoEntityEnable.szCardTitle = "ENTITY AO";
	aoEntityEnable.szCard =
		"Lays a soft shadow on the ground directly beneath monsters, props "
		"and the player, so they sit in the world instead of floating just "
		"above it.\n\n"
		"This is a cheap fake - a blurred blob that tracks the entity's "
		"feet - not a true cast shadow. For real shadows that follow the "
		"light, turn on Entity Shadows below.";
	aoEntityEnable.Setup( "r_ao", 1 );
	AddRow( TAB_ADVANCED, aoEntityEnable, ROW_H );

	aoCustomize.SetNameAndStatus( "Customize Ambient Occlusion...", NULL );
	aoCustomize.szHint = "Strength and detail for both the contact and the baked world AO";
	aoCustomize.onReleased = UI_ContAO_Menu;
	AddRow( TAB_ADVANCED, aoCustomize, ROW_H );

	hdrShadows.SetNameAndStatus( "DYNAMIC SHADOWS", NULL );
	AddRow( TAB_ADVANCED, hdrShadows, HEADER_H );

	entShadows.SetNameAndStatus( "Entity Shadows", NULL );
	entShadows.szHint = "Monsters, props and the player cast real shadows onto the world (experimental)";
	entShadows.szCardTitle = "ENTITY SHADOWS";
	entShadows.szCard =
		"Real depth-mapped shadows cast by monsters, props and the player "
		"onto the world and each other - the genuine article, shaped by the "
		"light, not the blob under Entity AO.\n\n"
		"Still experimental: it costs more than anything else here and can "
		"show edge artifacts in busy scenes. Customize Entity Shadows tunes "
		"softness, resolution and how many casters draw at once.";
	entShadows.Setup( "r_entity_shadows", 1 );
	AddRow( TAB_ADVANCED, entShadows, ROW_H );

	entShadowsCustomize.SetNameAndStatus( "Customize Entity Shadows...", NULL );
	entShadowsCustomize.szHint = "Strength, softness, resolution, caster cap and more";
	entShadowsCustomize.onReleased = UI_ContEntShadows_Menu;
	AddRow( TAB_ADVANCED, entShadowsCustomize, ROW_H );

	// --- GAMEPLAY tab (flashlight detail settings live on the Customize sub-page) ---
	alwaysRun.SetNameAndStatus( "Always Run", NULL );
	alwaysRun.szHint = "On: run by default, hold the speed key (Shift) to walk. Off: walk by default, hold to run";
	AddRow( TAB_GAMEPLAY, alwaysRun, ROW_H );

	flProjected.SetNameAndStatus( "Improved Flashlight", NULL );
	flProjected.szHint = "Projected-texture spotlight instead of the stock round blob";
	flProjected.szCardTitle = "IMPROVED FLASHLIGHT";
	flProjected.szCard =
		"Replaces the stock flashlight - a flat round patch of brightness "
		"pasted onto whatever you face - with a real projected spotlight: a "
		"proper cone that falls off with distance and casts its own shadows.\n\n"
		"Customize Flashlight tunes the beam shape, brightness, range and "
		"shadows. Infinite Battery removes the drain if you would rather not "
		"manage it.";
	flProjected.Setup( "r_flashlight_projected", 1 );
	AddRow( TAB_GAMEPLAY, flProjected, ROW_H );

	flInfinite.SetNameAndStatus( "Infinite Battery", NULL );
	flInfinite.szHint = "Flashlight never drains and never auto-shuts-off";
	flInfinite.Setup( "flashlight_infinite", 0 );
	AddRow( TAB_GAMEPLAY, flInfinite, ROW_H );

	flCustomize.SetNameAndStatus( "Customize Flashlight...", NULL );
	flCustomize.szHint = "Beam shape, brightness, range, shadows and more";
	flCustomize.onReleased = UI_ContFlashlight_Menu;
	AddRow( TAB_GAMEPLAY, flCustomize, ROW_H );

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
	msaa.Setup( "gl_msaa_samples", msaaLabels, msaaValues, 4, 1 );
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
	renderScale.szCardTitle = "RENDER SCALE";
	renderScale.szCard =
		"Renders the 3D scene at a fraction of the window resolution and "
		"upscales the result - trading sharpness for a large speed-up, and "
		"a chunky retro-pixel look at the higher multipliers.\n\n"
		"The HUD and menus stay crisp; only the world is scaled. 2x is a "
		"gentle boost; 4x on a small map is deliberately blocky but "
		"completely harmless.";
	renderScale.Setup( "vid_scale", scaleLabels, scaleValues, 4, 0 );
	AddRow( TAB_ADVANCED, renderScale, ROW_H );

	lodBias.SetNameAndStatus( "Mipmap Sharpness", NULL );
	lodBias.szHint = "Negative = sharper but shimmery";
	lodBias.bCaution = true;
	lodBias.Setup( "gl_texture_lodbias", -2.0f, 0.0f, 0.25f, 0.0f, 2 );
	AddRow( TAB_ADVANCED, lodBias, ROW_H );

	// CONSOLE section (Enable Console / Console Font / Console Font Size) lives
	// on the Interface tab — see the TAB_INTERFACE block in _Init above.

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
	// in-game: tint the rows' column, show the game behind the rest; out-of-game: the
	// current game's backdrop, like the other menu screens (was a flat fill before)
	DrawScreenBackdrop( CurrentGameBackdrop(), MARGIN - 30, ROW_W + 46 );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"CONFIGURATION", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		"SHARED BY ALL GAMES", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	// tab bar
	static const char *tabNames[TAB_COUNT] = { "VIDEO", "AUDIO", "CONTROLS", "INTERFACE", "GAMEPLAY", "ADVANCED" };
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
	int y = 138;

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
	// in-game: tint the rows' column, show the game behind the rest; out-of-game: the
	// current game's backdrop, like the other menu screens (was a flat fill before)
	DrawScreenBackdrop( CurrentGameBackdrop(), MARGIN - 30, ROW_W + 46 );

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

//=============================================================================
// Flashlight customize sub-page (opened from the Gameplay tab)
//=============================================================================
class CMenuContFlashlight : public CMenuFramework
{
public:
	CMenuContFlashlight() : CMenuFramework( "CMenuContFlashlight" ) { }

	bool KeyDown( int key ) override;
	void Draw() override;
	void Hide() override;

private:
	void _Init() override;
	void _VidInit() override;

	CContSliderRow beam, spillCone, spillBright, range, bright, vOffset, hOffset, shadowSize;
	CContToggleRow tint, shadows;
};

void CMenuContFlashlight::_Init()
{
	beam.SetNameAndStatus( "Beam Angle", NULL );
	beam.szHint = "Width of the bright hotspot, in degrees";
	beam.Setup( "r_flashlight_cone", 10, 90, 5, 35, 0 );
	AddItem( beam );

	spillCone.SetNameAndStatus( "Spill Angle", NULL );
	spillCone.szHint = "Width of the dimmer halo around the beam (the field)";
	spillCone.Setup( "r_flashlight_spill_cone", 20, 130, 5, 90, 0 );
	AddItem( spillCone );

	spillBright.SetNameAndStatus( "Spill Brightness", NULL );
	spillBright.szHint = "How bright the spill halo is, relative to the beam";
	spillBright.Setup( "r_flashlight_spill_intensity", 0.0f, 1.0f, 0.05f, 0.15f, 2 );
	AddItem( spillBright );

	range.SetNameAndStatus( "Range", NULL );
	range.szHint = "How far the beam reaches before it fades out";
	range.Setup( "r_flashlight_range", 400, 5000, 100, 3000, 0 );
	AddItem( range );

	bright.SetNameAndStatus( "Brightness", NULL );
	bright.szHint = "Beam brightness; above 1 adds extra additive passes for a much brighter light";
	bright.Setup( "r_flashlight_intensity", 0.2f, 6.0f, 0.2f, 3.0f, 1 );
	AddItem( bright );

	vOffset.SetNameAndStatus( "Vertical Offset", NULL );
	vOffset.szHint = "Light height vs the eye for shadow parallax: + above (headlamp), - below (chest)";
	vOffset.Setup( "r_flashlight_offset", -24, 24, 2, -4, 0 );
	AddItem( vOffset );

	hOffset.SetNameAndStatus( "Horizontal Offset", NULL );
	hOffset.szHint = "Light side vs the eye: + right (shoulder), - left, 0 centered (chest)";
	hOffset.Setup( "r_flashlight_offset_h", -24, 24, 2, -4, 0 );
	AddItem( hOffset );

	tint.SetNameAndStatus( "Tint by Surface", NULL );
	tint.szHint = "Beam reveals the surface texture instead of a flat glow";
	tint.Setup( "r_flashlight_albedo", 1 );
	AddItem( tint );

	shadows.SetNameAndStatus( "Cast Shadows", NULL );
	shadows.szHint = "The beam is blocked by walls, props and monsters";
	shadows.Setup( "r_flashlight_shadows", 1 );
	AddItem( shadows );

	shadowSize.SetNameAndStatus( "Shadow Resolution", NULL );
	shadowSize.szHint = "Shadow-map size in texels; higher = crisper shadow edges, more GPU (capped to the window size)";
	shadowSize.Setup( "r_flashlight_shadow_size", 256, 2048, 256, 512, 0 );
	AddItem( shadowSize );
}

void CMenuContFlashlight::_VidInit()
{
	VidInitFonts();

	const int itemH = 50, gap = 4;
	int y = 138;

	CContButton *rows[] = { &beam, &spillCone, &spillBright, &range, &bright, &vOffset, &hOffset, &tint, &shadows, &shadowSize };
	for( size_t i = 0; i < V_ARRAYSIZE( rows ); i++, y += itemH + gap )
		rows[i]->SetRect( MARGIN, y, ROW_W, itemH );
}

bool CMenuContFlashlight::KeyDown( int key )
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
		beam.ResetDefault(); spillCone.ResetDefault(); spillBright.ResetDefault();
		range.ResetDefault(); bright.ResetDefault(); vOffset.ResetDefault(); hOffset.ResetDefault();
		tint.ResetDefault(); shadows.ResetDefault(); shadowSize.ResetDefault();
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
		return true;
	}

	return CMenuFramework::KeyDown( key );
}

void CMenuContFlashlight::Hide()
{
	EngFuncs::ClientCmd( false, "host_writeconfig\n" );
	CMenuFramework::Hide();
}

void CMenuContFlashlight::Draw()
{
	// in-game: tint the rows' column, show the game behind the rest; out-of-game: the
	// current game's backdrop, like the other menu screens (was a flat fill before)
	DrawScreenBackdrop( CurrentGameBackdrop(), MARGIN - 30, ROW_W + 46 );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"FLASHLIGHT", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		"BEAM, SHADOWS AND MORE - SHARED BY ALL GAMES", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	CMenuFramework::Draw();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Change" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
		{ GLYPH_X, GLYPH_COUNT, "Restore Defaults" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ));
}

ADD_MENU( menu_continuum_flashlight, CMenuContFlashlight, UI_ContFlashlight_Menu );

//==========================================================================
// Ambient Occlusion sub-page (the master on/off lives on the Advanced tab)
//==========================================================================
class CMenuContAO : public CMenuFramework
{
public:
	CMenuContAO() : CMenuFramework( "CMenuContAO" ) { }

	bool KeyDown( int key ) override;
	void Draw() override;
	void Show() override;
	void Hide() override;

private:
	void _Init() override;
	void _VidInit() override;

	CContSliderRow aoStrength, aoSize, aoSoft, aoHeight, aoWorld, aoWorldRange, aoWorldMax;
	CContToggleRow aoEntityDbg, aoWorldDbg;
	CContButton aoRebake;	// manual "re-bake all maps"; only usable from the main menu
};

void CMenuContAO::_Init()
{
	aoStrength.SetNameAndStatus( "Contact Strength", NULL );
	aoStrength.szHint = "Darkness of the soft shadow under entities and props";
	aoStrength.Setup( "r_ao_strength", 0.0f, 1.0f, 0.05f, 0.5f, 2 );
	AddItem( aoStrength );

	aoSize.SetNameAndStatus( "Contact Footprint", NULL );
	aoSize.szHint = "Size of the contact footprint relative to the model";
	aoSize.Setup( "r_ao_size", 0.5f, 2.0f, 0.1f, 1.1f, 1 );
	AddItem( aoSize );

	aoSoft.SetNameAndStatus( "Contact Softness", NULL );
	aoSoft.szHint = "Edge blur of the contact shadow, in units";
	aoSoft.Setup( "r_ao_soft", 0, 12, 1, 2, 0 );
	AddItem( aoSoft );

	aoHeight.SetNameAndStatus( "Contact Height Falloff", NULL );
	aoHeight.szHint = "How far up a model contributes - feet darker than raised arms";
	aoHeight.Setup( "r_ao_height", 4, 64, 4, 16, 0 );
	AddItem( aoHeight );

	aoEntityDbg.SetNameAndStatus( "Entity Debug (Purple)", NULL );
	aoEntityDbg.szHint = "Draw entity contact-AO footprints in solid purple";
	aoEntityDbg.Setup( "r_ao_debug", 0 );
	AddItem( aoEntityDbg );

	aoWorld.SetNameAndStatus( "World Strength", NULL );
	aoWorld.szHint = "Darkness of the baked corner/recess shading on the world";
	aoWorld.Setup( "r_ao_world_strength", 0.0f, 1.0f, 0.05f, 0.6f, 2 );
	AddItem( aoWorld );

	aoWorldRange.SetNameAndStatus( "World Range", NULL );
	aoWorldRange.szHint = "How far world AO reaches into corners (use Re-bake to apply)";
	aoWorldRange.Setup( "r_ao_world_dist", 16, 256, 8, 64, 0 );
	AddItem( aoWorldRange );

	aoWorldMax.SetNameAndStatus( "World Max Darkness", NULL );
	aoWorldMax.szHint = "Cap on world AO so tight gaps don't go black";
	aoWorldMax.Setup( "r_ao_world_max", 0.0f, 1.0f, 0.05f, 0.6f, 2 );
	AddItem( aoWorldMax );

	aoWorldDbg.SetNameAndStatus( "World Debug (Pink)", NULL );
	aoWorldDbg.szHint = "Show the baked world AO as hot pink in the lightmap";
	aoWorldDbg.Setup( "r_ao_world_debug", 0 );
	AddItem( aoWorldDbg );

	aoRebake.SetNameAndStatus( "Re-bake World AO", NULL );
	aoRebake.szHint = "Re-run the world-AO bake for every map (main menu only)";
	aoRebake.szCardTitle = "RE-BAKE WORLD AO";
	aoRebake.szCard =
		"World AO is raycast offline and cached per map, so changing World "
		"Range doesn't take effect until the maps are re-baked.\n\n"
		"This re-bakes every map in the game at the current World Range. It "
		"loads each map in turn, so it can only run when no map is live - "
		"return to the MAIN MENU first (this button is disabled in-game).\n\n"
		"Baking can take a while; the game shows a progress screen. New "
		"caches are picked up the next time you load each map.";
	aoRebake.onReleased.SetCommand( false, "r_ao_bake_all\n" );
	AddItem( aoRebake );
}

void CMenuContAO::_VidInit()
{
	VidInitFonts();

	const int itemH = 44, gap = 4;	// 10 rows: slightly shorter than other sub-pages so they all fit above the legend
	int y = 138;

	CContButton *rows[] = { &aoStrength, &aoSize, &aoSoft, &aoHeight, &aoEntityDbg, &aoWorld, &aoWorldRange, &aoWorldMax, &aoWorldDbg, &aoRebake };
	for( size_t i = 0; i < V_ARRAYSIZE( rows ); i++, y += itemH + gap )
		rows[i]->SetRect( MARGIN, y, ROW_W, itemH );
}

bool CMenuContAO::KeyDown( int key )
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
		aoStrength.ResetDefault(); aoSize.ResetDefault(); aoSoft.ResetDefault(); aoHeight.ResetDefault();
		aoEntityDbg.ResetDefault(); aoWorld.ResetDefault(); aoWorldRange.ResetDefault();
		aoWorldMax.ResetDefault(); aoWorldDbg.ResetDefault();
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
		return true;
	}

	return CMenuFramework::KeyDown( key );
}

void CMenuContAO::Show()
{
	CMenuFramework::Show();
	// the all-maps bake loads each map in turn, so it can't run while a map is live;
	// gray the button out in-game (its card explains you must be at the main menu)
	aoRebake.SetGrayed( CL_IsActive( ));
}

void CMenuContAO::Hide()
{
	EngFuncs::ClientCmd( false, "host_writeconfig\n" );
	CMenuFramework::Hide();
}

void CMenuContAO::Draw()
{
	// in-game: tint the rows' column, show the game behind the rest; out-of-game: the
	// current game's backdrop, like the other menu screens (was a flat fill before)
	DrawScreenBackdrop( CurrentGameBackdrop(), MARGIN - 30, ROW_W + 46 );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"AMBIENT OCCLUSION", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		"CONTACT SHADOWS + BAKED WORLD AO", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	CMenuFramework::Draw();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Change" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
		{ GLYPH_X, GLYPH_COUNT, "Restore Defaults" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ));
}

ADD_MENU( menu_continuum_ao, CMenuContAO, UI_ContAO_Menu );

//-----------------------------------------------------------------------------
// Entity Shadows customize sub-page (opened from the Advanced tab; the master
// "Entity Shadows" toggle stays on the Advanced tab itself)
//-----------------------------------------------------------------------------
class CMenuContEntShadows : public CMenuFramework
{
public:
	CMenuContEntShadows() : CMenuFramework( "CMenuContEntShadows" ) { }

	bool KeyDown( int key ) override;
	void Draw() override;
	void Hide() override;

private:
	void _Init() override;
	void _VidInit() override;

	CContSliderRow esStrength, esSoft, esSize, esMax;
	CContToggleRow esPlayer, esDbg;
};

void CMenuContEntShadows::_Init()
{
	esStrength.SetNameAndStatus( "Shadow Strength", NULL );
	esStrength.szHint = "How dark the entity shadows are (0 = none, 1 = black)";
	esStrength.Setup( "r_entity_shadows_strength", 0.0f, 1.0f, 0.05f, 0.4f, 2 );
	AddItem( esStrength );

	esSoft.SetNameAndStatus( "Shadow Softness", NULL );
	esSoft.szHint = "Soften the shadow edge (0 = hard); box-blur radius in coverage texels";
	esSoft.Setup( "r_entity_shadows_softness", 0, 16, 1, 6, 0 );
	AddItem( esSoft );

	esSize.SetNameAndStatus( "Shadow Resolution", NULL );
	esSize.szHint = "Coverage-map size in texels; higher = finer footprint, more CPU";
	esSize.Setup( "r_entity_shadows_size", 64, 1024, 64, 256, 0 );
	AddItem( esSize );

	esMax.SetNameAndStatus( "Max Casters", NULL );
	esMax.szHint = "How many of the nearest entities cast a shadow; lower = faster";
	esMax.Setup( "r_entity_shadows_max", 2, 50, 1, 16, 0 );
	AddItem( esMax );

	esPlayer.SetNameAndStatus( "Player Casts Shadow", NULL );
	esPlayer.szHint = "Other players, and your own body in third-person, cast shadows (no body in first-person)";
	esPlayer.Setup( "r_entity_shadows_player", 1 );
	AddItem( esPlayer );

	esDbg.SetNameAndStatus( "Debug (Yellow)", NULL );
	esDbg.szHint = "Draw the shadow footprints in bright yellow to see where they land";
	esDbg.Setup( "r_entity_shadows_debug", 0 );
	AddItem( esDbg );
}

void CMenuContEntShadows::_VidInit()
{
	VidInitFonts();

	const int itemH = 50, gap = 4;
	int y = 138;

	CContButton *rows[] = { &esStrength, &esSoft, &esSize, &esMax, &esPlayer, &esDbg };
	for( size_t i = 0; i < V_ARRAYSIZE( rows ); i++, y += itemH + gap )
		rows[i]->SetRect( MARGIN, y, ROW_W, itemH );
}

bool CMenuContEntShadows::KeyDown( int key )
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
		esStrength.ResetDefault(); esSoft.ResetDefault(); esSize.ResetDefault(); esMax.ResetDefault();
		esPlayer.ResetDefault(); esDbg.ResetDefault();
		EngFuncs::PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
		return true;
	}

	return CMenuFramework::KeyDown( key );
}

void CMenuContEntShadows::Hide()
{
	EngFuncs::ClientCmd( false, "host_writeconfig\n" );
	CMenuFramework::Hide();
}

void CMenuContEntShadows::Draw()
{
	// in-game: tint the rows' column, show the game behind the rest; out-of-game: the
	// current game's backdrop, like the other menu screens (was a flat fill before)
	DrawScreenBackdrop( CurrentGameBackdrop(), MARGIN - 30, ROW_W + 46 );

	const int tx = MARGIN * uiStatic.scaleX;
	const int ty = 64 * uiStatic.scaleY;
	const int titleH = 30 * uiStatic.scaleY;
	const int subH = 12 * uiStatic.scaleY;

	UI_DrawString( fontTitle, tx, ty, ScreenWidth, titleH * 1.45f,
		"ENTITY SHADOWS", clrInk, titleH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );
	UI_DrawString( fontSmall, tx, ty + titleH + 8 * uiStatic.scaleY, ScreenWidth, subH * 1.45f,
		"SOFT DYNAMIC SHADOWS CAST BY MOVING ENTITIES (EXPERIMENTAL)", clrInkDim, subH, QM_LEFT, ETF_NOSIZELIMIT | ETF_FORCECOL );

	CMenuFramework::Draw();

	static const LegendEntry legend[] =
	{
		{ GLYPH_A, GLYPH_COUNT, "Change" },
		{ GLYPH_B, GLYPH_COUNT, "Back" },
		{ GLYPH_X, GLYPH_COUNT, "Restore Defaults" },
	};
	DrawLegend( legend, V_ARRAYSIZE( legend ));
}

ADD_MENU( menu_continuum_entshadows, CMenuContEntShadows, UI_ContEntShadows_Menu );
