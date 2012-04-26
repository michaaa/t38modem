/*
 * OptionsVideo.cpp
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

#include "stdafx.h"
#include "MobileOPAL.h"
#include "OptionsCodecs.h"


enum {
  CanEditItem    = 1,
  DragSourceItem = 2,
  DragTargetItem = 4
};


// COptionsCodecs dialog

IMPLEMENT_DYNAMIC(COptionsCodecs, CScrollableDialog)

COptionsCodecs::COptionsCodecs(CWnd* pParent /*=NULL*/)
  : CScrollableDialog(COptionsCodecs::IDD, pParent)
  , m_AutoStartTxVideo(FALSE)
  , m_hEnabledCodecs(NULL)
  , m_hDisabledCodecs(NULL)
  , m_hDraggedItem(NULL)
  , m_pDragImageList(NULL)
{

}

COptionsCodecs::~COptionsCodecs()
{
}

void COptionsCodecs::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  DDX_Check(pDX, IDC_AUTO_START_TX_VIDEO, m_AutoStartTxVideo);
  DDX_Control(pDX, IDC_CODEC_LIST, m_CodecInfo);
}


BEGIN_MESSAGE_MAP(COptionsCodecs, CDialog)
  ON_NOTIFY(TVN_BEGINLABELEDIT, IDC_CODEC_LIST, &COptionsCodecs::OnTvnBeginlabeleditCodecList)
  ON_NOTIFY(TVN_ENDLABELEDIT, IDC_CODEC_LIST, &COptionsCodecs::OnTvnEndlabeleditCodecList)
  ON_NOTIFY(TVN_BEGINDRAG, IDC_CODEC_LIST, &COptionsCodecs::OnTvnBegindragCodecList)
  ON_WM_CONTEXTMENU()
  ON_WM_MOUSEMOVE()
  ON_WM_LBUTTONUP()
END_MESSAGE_MAP()


static void SplitString(const CString & str, CMediaInfoList & list)
{
  int tokenPos = 0;
  CString token;
  while (!(token = str.Tokenize(L"\n", tokenPos)).IsEmpty()) {
    CMediaInfo info;

    int colon = token.Find(':');
    if (colon < 0)
      info = token;
    else {
      info = token.Left(colon);

      int equal = token.FindOneOf(L"#=");
      if (equal < 0)
        info.m_options[token.Mid(colon+1)] = CMediaInfo::OptionInfo();
      else
        info.m_options[token.Mid(colon+1, equal-colon-1)] = CMediaInfo::OptionInfo(token.Mid(equal+1), token[equal] == '#');
    }

    CMediaInfoList::iterator iter = std::find(list.begin(), list.end(), info);
    if (iter == list.end())
      list.push_back(info);
    else if (!info.m_options.empty())
      iter->m_options[info.m_options.begin()->first] = info.m_options.begin()->second;
  }
}


BOOL COptionsCodecs::OnInitDialog()
{
  CScrollableDialog::OnInitDialog();

  m_hEnabledCodecs = m_CodecInfo.InsertItem(L"Enabled Codecs");
  m_hDisabledCodecs = m_CodecInfo.InsertItem(L"Disabled Codecs");

  m_CodecInfo.SetItemData(m_hEnabledCodecs, DragTargetItem);
  m_CodecInfo.SetItemData(m_hDisabledCodecs, DragTargetItem);

  CMediaInfoList order;
  SplitString(m_MediaOrder, order);

  CMediaInfoList mask;
  SplitString(m_MediaMask, mask);

  CMediaInfoList options;
  SplitString(m_MediaOptions, options);

  CMediaInfoList all;
  SplitString(m_AllMediaOptions, all);

  CMediaInfoList::const_iterator iter;
  for (iter = order.begin(); iter != order.end(); ++iter) {
    CMediaInfoList::iterator full = std::find(all.begin(), all.end(), *iter);
    if (full != all.end())
      AddCodec(*full, mask, options);
  }

  for (iter = all.begin(); iter != all.end(); ++iter) {
    if (std::find(order.begin(), order.end(), *iter) == order.end())
      AddCodec(*iter, mask, options);
  }

  m_CodecInfo.Expand(m_hEnabledCodecs, TVE_EXPAND);

  return TRUE;
}


void COptionsCodecs::AddCodec(const CMediaInfo & info, const CMediaInfoList & mask, const CMediaInfoList & options)
{
  CMediaInfo adjustedInfo = info;

  CMediaInfoList::const_iterator overrides = std::find(options.begin(), options.end(), info);
  if (overrides != options.end()) {
    for (CMediaInfo::OptionMap::const_iterator iter = overrides->m_options.begin(); iter != overrides->m_options.end(); ++iter)
      adjustedInfo.m_options[iter->first] = iter->second;
  }

  HTREEITEM hFormat = m_CodecInfo.InsertItem(adjustedInfo,
                    std::find(mask.begin(), mask.end(), info) != mask.end() ? m_hDisabledCodecs : m_hEnabledCodecs);
  m_CodecInfo.SetItemData(hFormat, DragSourceItem|DragTargetItem);

  for (CMediaInfo::OptionMap::const_iterator iter = adjustedInfo.m_options.begin(); iter != adjustedInfo.m_options.end(); ++iter) {
    HTREEITEM hOptVal = m_CodecInfo.InsertItem(iter->second.m_value, m_CodecInfo.InsertItem(iter->first, hFormat));
    if (iter->second.m_readOnly)
      m_CodecInfo.SetItemState(hOptVal, TVIS_BOLD, TVIS_BOLD);
    else
      m_CodecInfo.SetItemData(hOptVal, CanEditItem);
  }
}


// COptionsCodecs message handlers

void COptionsCodecs::OnTvnBeginlabeleditCodecList(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
  *pResult = pTVDispInfo->item.cChildren != 0 || (pTVDispInfo->item.lParam&CanEditItem) == 0;
}


void COptionsCodecs::OnTvnEndlabeleditCodecList(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);

  if (pTVDispInfo->item.pszText == NULL)
    return;

  HTREEITEM hOption = m_CodecInfo.GetParentItem(pTVDispInfo->item.hItem);
  HTREEITEM hFormat = m_CodecInfo.GetParentItem(hOption);
  CString prefix = m_CodecInfo.GetItemText(hFormat) + ':' + m_CodecInfo.GetItemText(hOption) + '=';

  int pos = m_MediaOptions.Find(prefix);
  if (pos < 0) {
    if (!m_MediaOptions.IsEmpty())
      m_MediaOptions += '\n';
    m_MediaOptions += prefix + pTVDispInfo->item.pszText;
  }
  else {
    pos += prefix.GetLength();
    int end = m_MediaOptions.Find('\n', pos);
    if (end < 0)
      end = m_MediaOptions.GetLength();
    m_MediaOptions.Delete(pos, end-pos);
    m_MediaOptions.Insert(pos, pTVDispInfo->item.pszText);
  }

  *pResult = 1;
}


void COptionsCodecs::OnTvnBegindragCodecList(NMHDR *pNMHDR, LRESULT *pResult)
{
  *pResult = 0;
  LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

  if (m_hDraggedItem != NULL)
    return;

  if ((pNMTreeView->itemNew.lParam&DragSourceItem) != 0)
    StartDrag(pNMTreeView->itemNew.hItem, pNMTreeView->ptDrag);
}

void COptionsCodecs::OnMouseMove(UINT nFlags, CPoint point)
{
  CScrollableDialog::OnMouseMove(nFlags, point);

  if (m_hDraggedItem == NULL)
    return;

  if (m_pDragImageList != NULL) {
    m_pDragImageList->DragMove(point);
    m_pDragImageList->DragShowNolock(FALSE);
  }

  m_CodecInfo.SelectDropTarget(DragTest(point));

  if (m_pDragImageList != NULL)
    m_pDragImageList->DragShowNolock(TRUE);
}

void COptionsCodecs::OnLButtonUp(UINT nFlags, CPoint point)
{
  CScrollableDialog::OnLButtonUp(nFlags, point);

  if (m_hDraggedItem == NULL)
    return;

  ReleaseCapture();

  if (m_pDragImageList != NULL) {
    m_pDragImageList->EndDrag();
    delete m_pDragImageList;
    m_pDragImageList = NULL;
  }

  m_CodecInfo.SelectDropTarget(NULL);

  HTREEITEM hTargetItem = DragTest(point);
  if (hTargetItem != NULL)
    Move(m_hDraggedItem, hTargetItem);

  m_hDraggedItem = NULL;
}

void COptionsCodecs::OnContextMenu(CWnd * pWnd, CPoint point)
{
  if (&m_CodecInfo != pWnd)
    return;

  CPoint local = point;
  m_CodecInfo.ScreenToClient(&local);

  UINT nFlags;
  HTREEITEM hTargetItem = m_CodecInfo.HitTest(local, &nFlags);
  if (hTargetItem == NULL)
    return;

  if ((m_CodecInfo.GetItemData(hTargetItem)&DragSourceItem) == 0)
    return;

  m_CodecInfo.Select(hTargetItem, TVGN_CARET);

  CMenu menu;
  menu.CreatePopupMenu();
  menu.AppendMenu(MF_STRING, 1, L"Move");
  if (m_CodecInfo.GetParentItem(hTargetItem) == m_hDisabledCodecs)
    menu.AppendMenu(MF_STRING, 2, L"Enable");
  else
    menu.AppendMenu(MF_STRING, 3, L"Disable");

  switch (menu.TrackPopupMenu(TPM_LEFTALIGN|TPM_RETURNCMD, point.x, point.y, this)) {
    case 1 :
      StartDrag(hTargetItem, point);
      break;
    case 2 :
      Move(hTargetItem, m_hEnabledCodecs);
      break;
    case 3 :
      Move(hTargetItem, m_hDisabledCodecs);
      break;
  }
}

void COptionsCodecs::StartDrag(HTREEITEM hItem, CPoint point)
{
  SetCapture();

  m_hDraggedItem = hItem;
  m_CodecInfo.Select(m_hDraggedItem, TVGN_CARET);

  m_pDragImageList = m_CodecInfo.CreateDragImage(hItem);
  if (m_pDragImageList == NULL)
    return;

  m_pDragImageList->DragEnter(&m_CodecInfo, point);
  m_pDragImageList->BeginDrag(0, CPoint(0, 0));
}

HTREEITEM COptionsCodecs::DragTest(CPoint point)
{
  CRect dlgRect, treeRect;
  GetWindowRect(&dlgRect);
  m_CodecInfo.GetWindowRect(&treeRect);

  point -= treeRect.TopLeft() - dlgRect.TopLeft();

  UINT nFlags;
  HTREEITEM hTargetItem = m_CodecInfo.HitTest(point, &nFlags);
  if (hTargetItem == NULL)
    return NULL;

  if (hTargetItem == m_hDraggedItem)
    return NULL;

  if ((m_CodecInfo.GetItemData(hTargetItem)&DragTargetItem) == 0)
    return NULL;

  return hTargetItem;
}

void COptionsCodecs::Move(HTREEITEM hSource, HTREEITEM hDestination)
{
  CString formatName = m_CodecInfo.GetItemText(hSource);

  HTREEITEM hParent, hAfter;
  if (hDestination == m_hEnabledCodecs || hDestination == m_hDisabledCodecs) {
    hParent = hDestination;
    hAfter = TVI_FIRST;
  }
  else {
    hParent = m_CodecInfo.GetParentItem(hDestination);
    hAfter = hDestination;
  }

  HTREEITEM hNewFormat = m_CodecInfo.InsertItem(formatName, hParent, hAfter);
  m_CodecInfo.SetItemData(hNewFormat, DragSourceItem|DragTargetItem);

  for (HTREEITEM hOldOption = m_CodecInfo.GetChildItem(hSource);
                 hOldOption != NULL;
                 hOldOption = m_CodecInfo.GetNextItem(hOldOption, TVGN_NEXT)) {
    HTREEITEM hNewOption = m_CodecInfo.InsertItem(m_CodecInfo.GetItemText(hOldOption), hNewFormat);

    for (HTREEITEM hOldValue = m_CodecInfo.GetChildItem(hOldOption);
                   hOldValue != NULL;
                   hOldValue = m_CodecInfo.GetNextItem(hOldValue, TVGN_NEXT)) {
      HTREEITEM hNewValue = m_CodecInfo.InsertItem(m_CodecInfo.GetItemText(hOldValue), hNewOption);
      if ((m_CodecInfo.GetItemData(hOldValue)&CanEditItem) == 0)
        m_CodecInfo.SetItemState(hNewValue, TVIS_BOLD, TVIS_BOLD);
      else
        m_CodecInfo.SetItemData(hNewValue, CanEditItem);
    }
  }

  m_CodecInfo.DeleteItem(hSource);

  if (m_MediaMask[m_MediaMask.GetLength()-1] != '\n')
    m_MediaMask += '\n';

  formatName += '\n';
  int pos = m_MediaMask.Find(formatName);
  if (pos >= 0)
    m_MediaMask.Delete(pos, formatName.GetLength());

  if (m_CodecInfo.GetParentItem(hNewFormat) == m_hDisabledCodecs)
    m_MediaMask += formatName;

  m_MediaOrder.Empty();
  for (HTREEITEM hFormat = m_CodecInfo.GetChildItem(m_hEnabledCodecs);
                 hFormat != NULL;
                 hFormat = m_CodecInfo.GetNextItem(hFormat, TVGN_NEXT))
    m_MediaOrder += m_CodecInfo.GetItemText(hFormat) + '\n';
}
