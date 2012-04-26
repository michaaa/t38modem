/*
 * ScrollableDialog.h
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

#pragma once
#include "afxwin.h"


// CScrollableDialog class

class CScrollableDialog : public CDialog
{
  DECLARE_DYNAMIC(CScrollableDialog)

public:
  CScrollableDialog(int id, CWnd* pParent = NULL);   // standard constructor
  virtual ~CScrollableDialog();

protected:
  CCommandBar m_dlgCommandBar;

  virtual BOOL OnInitDialog();

  DECLARE_MESSAGE_MAP()

  afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
};
