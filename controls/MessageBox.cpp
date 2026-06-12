/*
MessageBox.cpp -- simple messagebox with text
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
#include "Utils.h"
#include "Action.h"
#include "ItemsHolder.h"
#include "MessageBox.h"
#include "continuum/Continuum.h"

CMenuMessageBox::CMenuMessageBox(const char *name) : BaseClass( name )
{
	iFlags |= QMF_INACTIVE;
}

void CMenuMessageBox::_Init()
{
	dlgMessage.eTextAlignment = QM_CENTER; // center
	dlgMessage.iFlags = QMF_INACTIVE|QMF_DROPSHADOW;
	dlgMessage.SetCoord( 0, 0 );
	dlgMessage.size = size;

	AddItem( dlgMessage );
}

void CMenuMessageBox::Draw()
{
	if( EngFuncs::GetCvarFloat( "ui_classic" ) != 0.0f )
	{
		// stock look for the classic menu family
		dlgMessage.font = uiStatic.hDefaultFont;
		dlgMessage.colorBase = uiPromptTextColor;

		EngFuncs::FillRGBA( m_scPos.x, m_scPos.y, m_scSize.w, m_scSize.h, 20, 20, 20, 235 );
		UI_DrawRectangle( m_scPos, m_scSize, uiInputFgColor );
		BaseClass::Draw();
		return;
	}

	// Continuum panel, matching CMenuYesNoMessageBox
	Cont::VidInitFonts();
	dlgMessage.font = Cont::fontBody;
	dlgMessage.colorBase = Cont::clrInk;

	UI_FillRect( m_scPos.x, m_scPos.y, m_scSize.w, m_scSize.h, 0xF20E1014 );
	UI_DrawRectangleExt( m_scPos.x, m_scPos.y, m_scSize.w, m_scSize.h, 0x28FFFFFF, 1 );
	UI_FillRect( m_scPos.x, m_scPos.y, m_scSize.w, 3 * uiStatic.scaleY, Cont::clrAccent );
	BaseClass::Draw();
}

void CMenuMessageBox::SetMessage( const char *sz )
{
	dlgMessage.szName = sz;
}
