/* This file is part of the KDE projects
   Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include "konqdrag.h"

KonqIconDrag::KonqIconDrag( QWidget * dragSource, const char* name )
  : QIconDrag( dragSource, name ),
    m_bCutSelection( false )
{
}

const char* KonqIconDrag::format( int i ) const
{
    if ( i == 0 )
	return "application/x-qiconlist";
    else if ( i == 1 )
	return "text/uri-list";
    else if ( i == 2 )
        return "application/x-kde-cutselection";
    else return 0;
}

QByteArray KonqIconDrag::encodedData( const char* mime ) const
{
    QByteArray a;
    QCString mimetype( mime );
    if ( mimetype == "application/x-qiconlist" )
	a = QIconDrag::encodedData( mime );
    else if ( mimetype == "text/uri-list" ) {
        QCString s = urls.join( "\r\n" ).latin1();
        // perhaps use QUriDrag::unicodeUriToUri here ?
	a.resize( s.length() );
	memcpy( a.data(), s.data(), s.length() );
    }
    else if ( mimetype == "application/x-kde-cutselection" ) {
        QCString s ( m_bCutSelection ? "1" : "0" );
	a.resize( s.length() );
	memcpy( a.data(), s.data(), s.length() );
    }
    return a;
}

bool KonqIconDrag::canDecode( const QMimeSource* e )
{
    return  e->provides( "application/x-qiconlist" ) ||
      e->provides( "text/uri-list" ) ||
      e->provides( "application/x-kde-cutselection" );
}

void KonqIconDrag::append( const QIconDragItem &item, const QRect &pr,
                             const QRect &tr, const QString &url )
{
    QIconDrag::append( item, pr, tr );
    urls.append(url);
}

//

KonqDrag::KonqDrag( QWidget * dragSource, const char* name )
  : QUriDrag( dragSource, name )
{}

const char* KonqDrag::format( int i ) const
{
    if ( i == 0 )
	return "text/uri-list";
    else if ( i == 1 )
        return "application/x-kde-cutselection";
    else return 0;
}

QByteArray KonqDrag::encodedData( const char* mime ) const
{
    QByteArray a;
    QCString mimetype( mime );
    if ( mimetype == "text/uri-list" )
        return QUriDrag::encodedData( mime );
    else if ( mimetype == "application/x-kde-cutselection" ) {
        QCString s ( m_bCutSelection ? "1" : "0" );
	a.resize( s.length() );
	memcpy( a.data(), s.data(), s.length() );
    }
    return a;
}

//

// Those are used for KonqIconDrag too

bool KonqDrag::decode( const QMimeSource *e, KURL::List &uris )
{
    QStrList lst;
    bool ret = QUriDrag::decode( e, lst );
    for (QStrListIterator it(lst); *it; ++it)
      uris.append(KURL(*it)); // *it is encoded already
    return ret;
}

bool KonqDrag::decodeIsCutSelection( const QMimeSource *e )
{
  QByteArray a = e->encodedData( "application/x-kde-cutselection" );
  if ( a.isEmpty() )
    return false;
  else
    return QCString( a ) = "1";
}

#include "konqdrag.moc"
