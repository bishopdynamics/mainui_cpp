/*
YesNoMessageBox.h - simple generic yes/no message box
Copyright (C) 2017 a1batross

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "extdll_menu.h"
#include "BaseMenu.h"
#include "Action.h"
#include "PicButton.h"
#include "YesNoMessageBox.h"
#include "Utils.h"
#include "FontManager.h"
#include "continuum/Continuum.h"

static void ToggleInactiveInternalCb( CMenuBaseItem *pSelf, void *pExtra );

/*
==============
CMenuDialogButton::Draw

flat Continuum-style button: focused gets the soft fill + accent left edge,
label in the Michroma item font
==============
*/
void CMenuDialogButton::Draw()
{
	if( EngFuncs::GetCvarFloat( "ui_classic" ) != 0.0f )
	{
		CMenuPicButton::Draw();
		return;
	}

	Cont::VidInitFonts(); // cheap, the builder dedups per video mode

	const bool focused = IsCurrentSelected();
	const bool grayed = FBitSet( iFlags, QMF_GRAYED );

	const int x = m_scPos.x;
	const int y = m_scPos.y;
	const int w = m_scSize.w;
	const int h = m_scSize.h;

	if( focused && !grayed )
	{
		UI_FillRect( x, y, w, h, Cont::clrAccentSoft );
		UI_FillRect( x, y, 3 * uiStatic.scaleX, h, Cont::clrAccent );
	}
	else
	{
		UI_FillRect( x, y, w, h, 0x14FFFFFF );
	}

	unsigned int color = grayed ? Cont::clrInkFaint : ( focused ? Cont::clrInk : Cont::clrInkDim );

	// bPulse marks the recommended choice (HighlightChoice)
	if( bPulse && !focused )
		color = PackAlpha( Cont::clrAccent, 255 * ( 0.65f + 0.35f * sin( (float)uiStatic.realTime / UI_PULSE_DIVISOR )));

	const int textH = 15 * uiStatic.scaleY;
	UI_DrawString( Cont::fontItem, x, y + ( h - textH ) / 2, w, textH * 1.45f,
		szName, color, textH, QM_CENTER, ETF_NOSIZELIMIT | ETF_FORCECOL | ETF_NO_WRAP );
}

CMenuYesNoMessageBox::CMenuYesNoMessageBox( bool alert ) : BaseClass( "YesNoMessageBox")
{
	bAutoHide = true;
	m_bIsAlert = alert;
	iFlags |= QMF_DIALOG;

	dlgMessage1.iFlags = QMF_INACTIVE|QMF_DROPSHADOW;
	dlgMessage1.eTextAlignment = QM_TOP;
	dlgMessage1.SetRect( 0, 24, 640, 256 - 24 );
	dlgMessage1.SetCharSize( QM_DEFAULTFONT );

	if( m_bIsAlert )
	{
		yes.SetRect( 298, 204, UI_BUTTONS_WIDTH / 2, UI_BUTTONS_HEIGHT );
	}
	else
	{
		yes.SetRect( 188, 204, UI_BUTTONS_WIDTH / 2, UI_BUTTONS_HEIGHT );
	}
	no.SetRect( 338, 204, UI_BUTTONS_WIDTH / 2, UI_BUTTONS_HEIGHT );

	yes.onReleased.pExtra = no.onReleased.pExtra = this;
	yes.bEnableTransitions = no.bEnableTransitions = false;

	SET_EVENT_MULTI( yes.onReleased,
	{
		CMenuYesNoMessageBox *msgBox = (CMenuYesNoMessageBox*)pExtra;

		if( msgBox->bAutoHide ) msgBox->Hide();
		msgBox->onPositive( msgBox );

	});

	SET_EVENT_MULTI( no.onReleased,
	{
		CMenuYesNoMessageBox *msgBox = (CMenuYesNoMessageBox*)pExtra;

		if( msgBox->bAutoHide ) msgBox->Hide();
		msgBox->onNegative( msgBox );
	});

	m_bSetYes = m_bSetNo = false;
	m_bIsAlert = alert;

	szName = "CMenuYesNoMessageBox";
}

/*
==============
CMenuYesNoMessageBox::Init
==============
*/
void CMenuYesNoMessageBox::_Init()
{
	SetRect( DLG_X + 192, 256, 640, 256 );

	if( !m_bSetYes )
		SetPositiveButton( L( "GameUI_OK" ), PC_OK );

	if( !m_bSetNo )
		SetNegativeButton( L( "GameUI_Cancel" ), PC_CANCEL );

	if( !(bool)onNegative )
		onNegative = CEventCallback::NoopCb;

	if( !(bool)onPositive )
		onPositive = CEventCallback::NoopCb;

	AddItem( dlgMessage1 );
	AddItem( yes );

	// alert dialog has single OK button
	if( !m_bIsAlert )
		AddItem( no );
}

/*
==============
CMenuYesNoMessageBox::VidInit
==============
*/
void CMenuYesNoMessageBox::_VidInit()
{
	SetRect( DLG_X + 192, 256, 640, 256 );
	pos.x += uiStatic.xOffset;
	pos.y += uiStatic.yOffset;
	CalcPosition();
	CalcSizes();

	// message font/colors are assigned per-Draw (they follow ui_classic)
	dlgMessage1.charSize = 17;
}

/*
==============
CMenuYesNoMessageBox::Show

gamepad-first: land focus on the positive choice so A/Enter confirms
without having to navigate first
==============
*/
void CMenuYesNoMessageBox::Show()
{
	BaseClass::Show();

	FOR_EACH_VEC( m_pItems, i )
	{
		if( m_pItems[i] == &yes )
		{
			SetCursor( i );
			// SetCursor only moves the index; the highlight is driven by
			// the focus bit, so set it too or the selection is invisible
			SetBits( yes.iFlags, QMF_HASKEYBOARDFOCUS );
			ClearBits( no.iFlags, QMF_HASKEYBOARDFOCUS );
			break;
		}
	}
}

/*
==============
CMenuYesNoMessageBox::Draw
==============
*/
void CMenuYesNoMessageBox::Draw()
{
	if( EngFuncs::GetCvarFloat( "ui_classic" ) != 0.0f )
	{
		// stock look for the classic menu family
		dlgMessage1.font = uiStatic.hDefaultFont;
		dlgMessage1.colorBase = uiPromptTextColor;

		UI_FillRect( 0, 0, gpGlobals->scrWidth, gpGlobals->scrHeight, 0x40000000 );
		EngFuncs::FillRGBA( m_scPos.x, m_scPos.y, m_scSize.w, m_scSize.h, 20, 20, 20, 235 );
		UI_DrawRectangle( m_scPos, m_scSize, uiInputFgColor );

		CMenuBaseWindow::Draw();
		return;
	}

	// Continuum: deep scrim so the dialog reads against any backdrop
	Cont::VidInitFonts();
	dlgMessage1.font = Cont::fontBody;
	dlgMessage1.colorBase = Cont::clrInk;

	UI_FillRect( 0, 0, gpGlobals->scrWidth, gpGlobals->scrHeight, 0x96000000 );

	// near-black card, hairline border, accent top edge
	UI_FillRect( m_scPos.x, m_scPos.y, m_scSize.w, m_scSize.h, 0xF20E1014 );
	UI_DrawRectangleExt( m_scPos.x, m_scPos.y, m_scSize.w, m_scSize.h, 0x28FFFFFF, 1 );
	UI_FillRect( m_scPos.x, m_scPos.y, m_scSize.w, 3 * uiStatic.scaleY, Cont::clrAccent );

	CMenuBaseWindow::Draw();
}

/*
==============
CMenuYesNoMessageBox::Key
==============
*/
bool CMenuYesNoMessageBox::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ) )
	{
		Hide();
		onNegative( this );

		return true;
	}

	return BaseClass::KeyDown( key );
}

/*
==============
CMenuYesNoMessageBox::SetMessage
==============
*/
void CMenuYesNoMessageBox::SetMessage( const char *msg )
{
	dlgMessage1.szName = ( msg );
}

/*
==============
CMenuYesNoMessageBox::SetPositiveButton
==============
*/
void CMenuYesNoMessageBox::SetPositiveButton( const char *msg, EDefaultBtns buttonPic, int extrawidth )
{
	m_bSetYes = true;
	yes.szName = msg;
	yes.SetPicture( buttonPic );
	yes.SetRect(  (m_bIsAlert?298:188) - extrawidth / 2, 204, UI_BUTTONS_WIDTH / 2 + extrawidth, UI_BUTTONS_HEIGHT );
}

/*
==============
CMenuYesNoMessageBox::SetNegativeButton
==============
*/
void CMenuYesNoMessageBox::SetNegativeButton( const char *msg, EDefaultBtns buttonPic, int extrawidth )
{
	m_bSetNo = true;
	no.szName = msg;
	no.SetPicture( buttonPic );
	no.SetRect( 338 + extrawidth / 2, 204, UI_BUTTONS_WIDTH / 2 + extrawidth, UI_BUTTONS_HEIGHT );
}

/*
==============
CMenuYesNoMessageBox::HighlightChoice
==============
*/
void CMenuYesNoMessageBox::HighlightChoice( EHighlight yesno )
{
	if( yesno == NO_HIGHLIGHT )
	{
		yes.bPulse = no.bPulse = false;
	}
	else
	{
		yes.bPulse = yesno == HIGHLIGHT_YES;
		no.bPulse = yesno == HIGHLIGHT_NO;
	}
}

CEventCallback CMenuYesNoMessageBox::MakeOpenEvent()
{
	return CEventCallback( OpenCb, this );
}


/*
==============
CMenuYesNoMessageBox::ToggleInactiveCb
==============
*/
void CMenuYesNoMessageBox::OpenCb( CMenuBaseItem *, void *pExtra )
{
	ToggleInactiveInternalCb( (CMenuBaseItem*)pExtra, NULL );
}

/*
==============
CMenuYesNoMessageBox::ToggleInactiveCb
==============
*/
static void ToggleInactiveInternalCb( CMenuBaseItem *pSelf, void * )
{
	pSelf->ToggleVisibility();
}

void UI_ShowMessageBox( const char *text )
{
	static char msg[1024];
	static CMenuYesNoMessageBox msgBox( true );

	Q_strncpy( msg, text, sizeof( msg ));

	if( !UI_IsVisible() )
	{
		UI_Main_Menu();
		UI_SetActiveMenu( true );
	}

	if( strstr( msg, "m_ignore") || strstr( msg, "touch_enable" ) || strstr( msg, "joy_enable" ) )
	{
		static CMenuYesNoMessageBox msgBoxInputDev( false );
		static bool init;

		if( !init )
		{
			msgBoxInputDev.SetPositiveButton( L( "GameUI_OK" ), PC_OK, 100 );
			msgBoxInputDev.SetNegativeButton( L( "GameUI_Options" ), PC_CONFIG, -20 );
			msgBoxInputDev.onNegative = UI_InputDevices_Menu;
			msgBoxInputDev.yes.SetCoord( 200, 204 );

			init = true;
		}

		msgBoxInputDev.SetMessage( msg );
		msgBoxInputDev.Show();
		msgBoxInputDev.yes.SetCoord( 200, 204 );
		return;
	}

	msgBox.SetMessage( msg );
	msgBox.Show();
}

void UI_ShowMessageBox_f()
{
	UI_ShowMessageBox( EngFuncs::CmdArgv(1) );
}

ADD_COMMAND( menu_showmessagebox, UI_ShowMessageBox_f );
