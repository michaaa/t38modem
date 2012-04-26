/*
 * wxstring.h
 *
 * Adapter class for WX Widgets strings.
 *
 * Portable Tools Library
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
 * The Original Code is Portable Windows Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 22403 $
 * $Author: rjongbloed $
 * $Date: 2009-04-07 12:13:31 +0200 (Di, 07. Apr 2009) $
 */

#ifndef PTLIB_WXSTRING_H
#define PTLIB_WXSTRING_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

/**This class defines a class to bridge WX Widgets strings to PTLib strings.
 */
class PwxString : public wxString
{
  public:
    PwxString() { }
    PwxString(const wxString & str) : wxString(str) { }
    PwxString(const PString & str) : wxString(str, wxConvUTF8 ) { }
    PwxString(const PFilePath & fn) : wxString(fn, wxConvUTF8 ) { }
    PwxString(const char * str) : wxString(str, wxConvUTF8) { }
    PwxString(const OpalMediaFormat & fmt) : wxString(fmt, wxConvUTF8) { }
#if wxUSE_UNICODE
    PwxString(const wchar_t * wstr) : wxString(wstr) { }
#endif

    inline PwxString & operator=(const char * str)     { *this = wxString::wxString(str, wxConvUTF8); return *this; }
#if wxUSE_UNICODE
    inline PwxString & operator=(const wchar_t * wstr) { wxString::operator=(wstr); return *this; }
#endif
    inline PwxString & operator=(const wxString & str) { wxString::operator=(str); return *this; }
    inline PwxString & operator=(const PString & str)  { *this = wxString::wxString(str, wxConvUTF8); return *this; }

    inline bool operator==(const char * other)            const { return IsSameAs(wxString(other, wxConvUTF8)); }
#if wxUSE_UNICODE
    inline bool operator==(const wchar_t * other)         const { return IsSameAs(other); }
#endif
    inline bool operator==(const wxString & other)        const { return IsSameAs(other); }
    inline bool operator==(const PString & other)         const { return IsSameAs(wxString(other, wxConvUTF8)); }
    inline bool operator==(const PwxString & other)       const { return IsSameAs(other); }
    inline bool operator==(const OpalMediaFormat & other) const { return IsSameAs(wxString(other, wxConvUTF8)); }

    inline bool operator!=(const char * other)            const { return !IsSameAs(wxString(other, wxConvUTF8)); }
#if wxUSE_UNICODE
    inline bool operator!=(const wchar_t * other)         const { return !IsSameAs(other); }
#endif
    inline bool operator!=(const wxString & other)        const { return !IsSameAs(other); }
    inline bool operator!=(const PString & other)         const { return !IsSameAs(wxString(other, wxConvUTF8)); }
    inline bool operator!=(const PwxString & other)       const { return !IsSameAs(other); }
    inline bool operator!=(const OpalMediaFormat & other) const { return !IsSameAs(wxString(other, wxConvUTF8)); }

#if wxUSE_UNICODE
    inline PString p_str() const { return ToUTF8().data(); }
    inline operator PString() const { return ToUTF8().data(); }
    inline operator PFilePath() const { return ToUTF8().data(); }
    inline operator PIPSocket::Address() const { return PString(ToUTF8().data()); }
    inline friend ostream & operator<<(ostream & stream, const PwxString & string) { return stream << string.ToUTF8(); }
    inline friend wostream & operator<<(wostream & stream, const PwxString & string) { return stream << string.c_str(); }
#else
    inline PString p_str() const { return c_str(); }
    inline operator PString() const { return c_str(); }
    inline operator PFilePath() const { return c_str(); }
    inline operator PIPSocket::Address() const { return c_str(); }
    inline friend ostream & operator<<(ostream & stream, const PwxString & string) { return stream << string.c_str(); }
    inline friend wostream & operator<<(wostream & stream, const PwxString & string) { return stream << string.c_str(); }
#endif
};


#endif // PTLIB_WXSTRING_H


// End Of File ///////////////////////////////////////////////////////////////
