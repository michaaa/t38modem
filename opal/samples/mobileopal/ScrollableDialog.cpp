/*
 * ScrollableDialog.cpp
 *
 * Sample Windows Mobile application.
 *
 * Open Phone Abstraction Library
 *
 * Copyright (c) 2008 Vox Lucida
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Vox Lucida (Robert Jongbloed)
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 21752 $
 * $Author: rjongbloed $
 * $Date: 2008-12-10 03:48:17 +0100 (Mi, 10. Dez 2008) $
 */

#include "stdafx.h"
#include "MobileOPAL.h"
#include "ScrollableDialog.h"


// ScrollableDialog class

IMPLEMENT_DYNAMIC(CScrollableDialog, CDialog)

CScrollableDialog::CScrollableDialog(int id, CWnd* pParent /*=NULL*/)
  : CDialog(id, pParent)
{

}

CScrollableDialog::~CScrollableDialog()
{
}

BEGIN_MESSAGE_MAP(CScrollableDialog, CDialog)
  ON_WM_VSCROLL()
END_MESSAGE_MAP()


BOOL CScrollableDialog::OnInitDialog()
{
  CDialog::OnInitDialog();

  if (!m_dlgCommandBar.Create(this) || !m_dlgCommandBar.InsertMenuBar(IDR_DIALOGS)) {
    TRACE0("Failed to create CommandBar\n");
  }

  // Calculate scroll bar size
  int range = 0;
  CRect dlgBox;
  GetClientRect(&dlgBox);
  for (CWnd * pChild = GetWindow(GW_CHILD); pChild != NULL; pChild = pChild->GetNextWindow(GW_HWNDNEXT)) {
    CRect childBox;
    pChild->GetWindowRect(&childBox);
    ScreenToClient(&childBox);
    int y = childBox.bottom - dlgBox.Height() + 4;
    if (y > range)
      range = y;
  }
  SetScrollRange(SB_VERT, 0, range, 0);

  return TRUE;
}


// CScrollableDialog message handlers

void CScrollableDialog::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* /*pScrollBar*/)
{
  // calc new y position
  int y = GetScrollPos(SB_VERT);
  int yOrig = y;

  switch (nSBCode)
  {
  case SB_TOP:
    y = 0;
    break;
  case SB_BOTTOM:
    y = INT_MAX;
    break;
  case SB_LINEUP:
    y -= 10;
    break;
  case SB_LINEDOWN:
    y += 10;
    break;
  case SB_PAGEUP:
    y -= 100;
    break;
  case SB_PAGEDOWN:
    y += 100;
    break;
  case SB_THUMBTRACK:
    y = nPos;
    break;
  }

  SetScrollPos(SB_VERT, y);

  y = GetScrollPos(SB_VERT);
  if (y != yOrig) {
    /* Now ScrollWindow(0, yOrig-y) is SUPPOSED to move all the child windows
       and in normal Win32 it does, but in WinCE it doesn't. All documentation
       says that it does, but to damn well DOESN'T. */
    for (CWnd * pChild = GetWindow(GW_CHILD); pChild != NULL; pChild = pChild->GetNextWindow(GW_HWNDNEXT)) {
      CRect rect;
      pChild->GetWindowRect(&rect);
	  ScreenToClient(&rect);
	  pChild->SetWindowPos(NULL, rect.left, rect.top+(yOrig-y), 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
    }
  }
}
