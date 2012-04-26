/*
 * OptionsVideo.h
 *
 * Sample Windows Mobile application.
 *
 * Open Phone Abstraction Library
 *
 * Copyright (c) 2009 Vox Lucida
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
 * $Revision: 23884 $
 * $Author: rjongbloed $
 * $Date: 2009-12-21 05:39:02 +0100 (Mo, 21. Dez 2009) $
 */

#pragma once
#include "afxwin.h"
#include "ScrollableDialog.h"
#include "afxcmn.h"

#include <map>
#include <list>
#include <algorithm>


class CMediaInfo : public CString {
  public:
    CMediaInfo() { }
    CMediaInfo(const CString & str) : CString(str) { }
    CMediaInfo & operator=(const CString & str) { CString::operator=(str); return *this; }

    struct OptionInfo {
      OptionInfo() : m_readOnly(true) { }
      OptionInfo(const CString & val, bool ro) : m_value(val), m_readOnly(ro) { }

      CString m_value;
      bool    m_readOnly;
    };
    typedef std::map<CString, OptionInfo> OptionMap;
    OptionMap m_options;
};

typedef std::list<CMediaInfo> CMediaInfoList;


// COptionsCodecs dialog

class COptionsCodecs : public CScrollableDialog
{
  DECLARE_DYNAMIC(COptionsCodecs)

public:
  COptionsCodecs(CWnd* pParent = NULL);   // standard constructor
  virtual ~COptionsCodecs();

  // Dialog Data
  enum { IDD = IDD_OPTIONS_CODECS };

protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

  DECLARE_MESSAGE_MAP()

  virtual BOOL OnInitDialog();

  void AddCodec(const CMediaInfo & info, const CMediaInfoList & mask, const CMediaInfoList & options);
  void StartDrag(HTREEITEM hItem, CPoint point);
  HTREEITEM DragTest(CPoint point);
  void Move(HTREEITEM hSource, HTREEITEM hDest);

public:
  BOOL m_AutoStartTxVideo;
  CString m_MediaOrder;
  CString m_MediaMask;
  CString m_MediaOptions;
  CString m_AllMediaOptions;

protected:
  CTreeCtrl    m_CodecInfo;
  HTREEITEM    m_hEnabledCodecs;
  HTREEITEM    m_hDisabledCodecs;
  HTREEITEM    m_hDraggedItem;
  CImageList * m_pDragImageList;

protected:
  afx_msg void OnTvnBeginlabeleditCodecList(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnTvnEndlabeleditCodecList(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnTvnBegindragCodecList(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnContextMenu(CWnd * pWnd, CPoint point);
};
