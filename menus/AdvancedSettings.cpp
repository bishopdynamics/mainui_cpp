/*
AdvancedSettings.cpp -- classic-style pages exposing the Continuum advanced
settings, so the original menu has feature parity with the new one
Copyright (C) 2026 a1batross, James Bishop

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#include "Framework.h"
#include "Bitmap.h"
#include "PicButton.h"
#include "CheckBox.h"
#include "Slider.h"
#include "SpinControl.h"
#include "StringArrayModel.h"
#include "Action.h"

#define ART_BANNER "gfx/shell/head_advanced"

void UI_AdvSettings2_Menu( void );
extern bool g_bUiFamilySwitch; // menus/continuum/RootMenu.cpp

static const char *g_szAnisoNames[] = { "off", "2x", "4x", "8x", "16x" };
static const float g_flAnisoValues[] = { 1, 2, 4, 8, 16 };
static const char *g_szMsaaNames[] = { "off", "2x", "4x", "8x" };
static const float g_flMsaaValues[] = { 0, 2, 4, 8 };
static const char *g_szFpsNames[] = { "60", "72", "100", "120", "144", "165", "240", "no limit" };
static const float g_flFpsValues[] = { 60, 72, 100, 120, 144, 165, 240, 0 };

static int NearestIndex( float value, const float *values, int count )
{
	int best = 0;
	float bestDist = 1e9f;

	for( int i = 0; i < count; i++ )
	{
		const float d = fabs( value - values[i] );
		if( d < bestDist )
		{
			bestDist = d;
			best = i;
		}
	}
	return best;
}

/*
====================
page 1: renderer
====================
*/
class CMenuAdvSettings : public CMenuFramework
{
public:
	CMenuAdvSettings() : CMenuFramework( "CMenuAdvSettings" ) { }

private:
	void _Init() override;
	void _VidInit() override;
	void GetConfig();
	void WriteAniso();
	void WriteMsaa();
	void WriteFps();

	CMenuCheckBox detailTex;
	CMenuCheckBox overbright;
	CMenuCheckBox dynLights;
	CMenuCheckBox shadows;
	CMenuCheckBox lightExt;
	CMenuCheckBox ripple;
	CMenuCheckBox litWater;
	CMenuCheckBox fovAdjust;
	CMenuCheckBox texNearest;
	CMenuCheckBox lmNearest;

	CMenuSpinControl aniso;
	CMenuSpinControl msaa;
	CMenuSpinControl fpsMax;
	CMenuSpinControl renderScale;
	CMenuSpinControl decals;
	CMenuSlider ambient;
	CMenuSlider lodBias;
};

void CMenuAdvSettings::WriteAniso()
{
	EngFuncs::CvarSetValue( "gl_anisotropy", g_flAnisoValues[(int)aniso.GetCurrentValue()] );
}

void CMenuAdvSettings::WriteMsaa()
{
	const int idx = (int)msaa.GetCurrentValue();

	EngFuncs::CvarSetValue( "gl_msaa_samples", g_flMsaaValues[idx] );
	EngFuncs::CvarSetValue( "gl_msaa", idx > 0 ? 1.0f : 0.0f );
}

void CMenuAdvSettings::WriteFps()
{
	EngFuncs::CvarSetValue( "fps_max", g_flFpsValues[(int)fpsMax.GetCurrentValue()] );
}

void CMenuAdvSettings::GetConfig()
{
	detailTex.LinkCvar( "r_detailtextures" );
	overbright.LinkCvar( "gl_overbright" );
	dynLights.LinkCvar( "r_dynamic" );
	shadows.LinkCvar( "r_shadows" );
	lightExt.LinkCvar( "r_lighting_extended" );
	ripple.LinkCvar( "r_ripple" );
	litWater.LinkCvar( "gl_litwater_force" );
	fovAdjust.LinkCvar( "r_adjust_fov" );
	texNearest.LinkCvar( "gl_texture_nearest" );
	lmNearest.LinkCvar( "gl_lightmap_nearest" );

	renderScale.LinkCvar( "vid_scale", CMenuEditable::CVAR_VALUE );
	decals.LinkCvar( "r_decals", CMenuEditable::CVAR_VALUE );
	ambient.LinkCvar( "r_lighting_ambient" );
	lodBias.LinkCvar( "gl_texture_lodbias" );

	// list-mapped spins are written through the callbacks above
	int i = NearestIndex( EngFuncs::GetCvarFloat( "gl_anisotropy" ), g_flAnisoValues, V_ARRAYSIZE( g_flAnisoValues ));
	aniso.SetCurrentValue( i );
	aniso.ForceDisplayString( g_szAnisoNames[i] );

	i = NearestIndex( EngFuncs::GetCvarFloat( "gl_msaa_samples" ), g_flMsaaValues, V_ARRAYSIZE( g_flMsaaValues ));
	msaa.SetCurrentValue( i );
	msaa.ForceDisplayString( g_szMsaaNames[i] );

	i = NearestIndex( EngFuncs::GetCvarFloat( "fps_max" ), g_flFpsValues, V_ARRAYSIZE( g_flFpsValues ));
	fpsMax.SetCurrentValue( i );
	fpsMax.ForceDisplayString( g_szFpsNames[i] );
}

void CMenuAdvSettings::_Init()
{
	static CStringArrayModel anisoModel( g_szAnisoNames, V_ARRAYSIZE( g_szAnisoNames ));
	static CStringArrayModel msaaModel( g_szMsaaNames, V_ARRAYSIZE( g_szMsaaNames ));
	static CStringArrayModel fpsModel( g_szFpsNames, V_ARRAYSIZE( g_szFpsNames ));

	banner.SetPicture( ART_BANNER );

	detailTex.SetNameAndStatus( "Detail textures", NULL );
	overbright.SetNameAndStatus( "Overbright lighting", NULL );
	dynLights.SetNameAndStatus( "Dynamic lights", NULL );
	shadows.SetNameAndStatus( "Entity shadows", NULL );
	lightExt.SetNameAndStatus( "Extended light sampling", NULL );
	ripple.SetNameAndStatus( "Water ripples", NULL );
	litWater.SetNameAndStatus( "Lightmapped water", NULL );
	fovAdjust.SetNameAndStatus( "FOV correction", NULL );
	texNearest.SetNameAndStatus( "Nearest texture filtering", NULL );
	lmNearest.SetNameAndStatus( "Nearest lightmap filtering", NULL );

	CMenuCheckBox *checks[] =
	{
		&detailTex, &overbright, &dynLights, &shadows, &lightExt,
		&ripple, &litWater, &fovAdjust, &texNearest, &lmNearest
	};

	for( size_t n = 0; n < V_ARRAYSIZE( checks ); n++ )
	{
		checks[n]->bUpdateImmediately = true;
		checks[n]->SetCoord( 300, 230 + (int)n * 50 );
		AddItem( *checks[n] );
	}

	aniso.SetNameAndStatus( "Anisotropy", NULL );
	aniso.Setup( &anisoModel );
	aniso.onChanged = VoidCb( &CMenuAdvSettings::WriteAniso );

	msaa.SetNameAndStatus( "Anti-aliasing (restart)", NULL );
	msaa.Setup( &msaaModel );
	msaa.onChanged = VoidCb( &CMenuAdvSettings::WriteMsaa );

	fpsMax.SetNameAndStatus( "FPS limit", NULL );
	fpsMax.Setup( &fpsModel );
	fpsMax.onChanged = VoidCb( &CMenuAdvSettings::WriteFps );

	renderScale.SetNameAndStatus( "Render scale", NULL );
	renderScale.Setup( 1.0f, 4.0f, 1.0f );
	renderScale.bUpdateImmediately = true;

	decals.SetNameAndStatus( "Decal limit", NULL );
	decals.Setup( 512.0f, 8192.0f, 512.0f );
	decals.bUpdateImmediately = true;

	ambient.SetNameAndStatus( "Ambient light", NULL );
	ambient.Setup( 0.0f, 1.0f, 0.05f );
	ambient.bUpdateImmediately = true;

	lodBias.SetNameAndStatus( "Mipmap sharpness", NULL );
	lodBias.Setup( -2.0f, 0.0f, 0.25f );
	lodBias.bUpdateImmediately = true;

	AddItem( aniso );
	AddItem( msaa );
	AddItem( fpsMax );
	AddItem( renderScale );
	AddItem( decals );
	AddItem( ambient );
	AddItem( lodBias );

	AddItem( banner );
	AddButton( L( "Game & menu" ), L( "Streaming, console and menu-style settings" ), PC_ADV_OPT, UI_AdvSettings2_Menu, QMF_NOTIFY );
	AddButton( L( "Done" ), L( "Go back to the previous menu" ), PC_DONE, VoidCb( &CMenuAdvSettings::Hide ), QMF_NOTIFY );
}

void CMenuAdvSettings::_VidInit()
{
	// toggles at 300 (set in _Init), value spins at 760, sliders at 1060;
	// spin/slider labels render above the widget
	int y = 250;

	aniso.SetRect( 760, y, 260, 32 );
	msaa.SetRect( 760, y += 70, 260, 32 );
	fpsMax.SetRect( 760, y += 70, 260, 32 );
	renderScale.SetRect( 760, y += 70, 260, 32 );
	decals.SetRect( 760, y += 70, 260, 32 );

	ambient.SetCoord( 1060, 250 );
	lodBias.SetCoord( 1060, 320 );

	GetConfig();
}

ADD_MENU( menu_advsettings, CMenuAdvSettings, UI_AdvSettings_Menu );

/*
====================
page 2: game, console and menu style
====================
*/
class CMenuAdvSettings2 : public CMenuFramework
{
public:
	CMenuAdvSettings2() : CMenuFramework( "CMenuAdvSettings2" ) { }

	void FocusUiToggle();

private:
	void _Init() override;
	void _VidInit() override;
	void ToggleUiFamily();

	CMenuCheckBox levelStreaming;
	CMenuCheckBox conEnable;
	CMenuCheckBox conTtf;
	CMenuCheckBox classicUi;
	CMenuSlider conFontSize;
};

void CMenuAdvSettings2::ToggleUiFamily()
{
	// write the cvar ourselves: onChanged can fire before the immediate
	// write, and the menu rebuild below must see the new value
	EngFuncs::CvarSetValue( "ui_classic", classicUi.bChecked ? 1.0f : 0.0f );

	g_bUiFamilySwitch = true;
	UI_CloseMenu();
	UI_SetActiveMenu( true ); // reopens through UI_Main_Menu -> new family
}

void CMenuAdvSettings2::FocusUiToggle()
{
	FOR_EACH_VEC( m_pItems, i )
	{
		if( m_pItems[i] == &classicUi )
		{
			SetCursor( i );
			break;
		}
	}
}

void CMenuAdvSettings2::_Init()
{
	banner.SetPicture( ART_BANNER );

	levelStreaming.SetNameAndStatus( "Level streaming", NULL );
	levelStreaming.bUpdateImmediately = true;

	conEnable.SetNameAndStatus( "Enable console", NULL );
	conEnable.bUpdateImmediately = true;

	conTtf.SetNameAndStatus( "TrueType console font", NULL );
	conTtf.bUpdateImmediately = true;

	classicUi.SetNameAndStatus( "Classic menu", NULL );
	classicUi.onChanged = VoidCb( &CMenuAdvSettings2::ToggleUiFamily );

	conFontSize.SetNameAndStatus( "Console font size", NULL );
	conFontSize.Setup( 1.0f, 2.5f, 0.1f );
	conFontSize.bUpdateImmediately = true;

	AddItem( levelStreaming );
	AddItem( conEnable );
	AddItem( conTtf );
	AddItem( classicUi );
	AddItem( conFontSize );

	AddItem( banner );
	AddButton( L( "Done" ), L( "Go back to the previous menu" ), PC_DONE, VoidCb( &CMenuAdvSettings2::Hide ), QMF_NOTIFY );
}

void CMenuAdvSettings2::_VidInit()
{
	levelStreaming.SetCoord( 360, 230 );
	conEnable.SetCoord( 360, 300 );
	conTtf.SetCoord( 360, 370 );
	classicUi.SetCoord( 360, 440 );
	conFontSize.SetCoord( 360, 550 );

	levelStreaming.LinkCvar( "host_level_streaming" );
	conEnable.LinkCvar( "con_enable" );
	conTtf.LinkCvar( "con_ttffont" );
	classicUi.LinkCvar( "ui_classic" );
	conFontSize.LinkCvar( "con_fontscale" );
}

ADD_MENU( menu_advsettings2, CMenuAdvSettings2, UI_AdvSettings2_Menu );

/*
================
UI_AdvSettings2_FocusUiToggle

family-switch landing spot: this page with the menu-style toggle focused,
so the activate button can flip the menu family back again immediately
================
*/
void UI_AdvSettings2_FocusUiToggle( void )
{
	UI_AdvSettings2_Menu();
	menu_advsettings2->FocusUiToggle();
}
