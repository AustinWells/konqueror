/* This file is part of the KDE project
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
#ifndef __konq_treeviewwidget_h__
#define __konq_treeviewwidget_h__

#include "konq_listviewwidget.h"
#include "konq_treeviewitem.h"
#include <qdict.h>
#include <kurl.h>
#include <klistview.h>

class KonqListView;

class KonqTreeViewWidget : public KonqBaseListViewWidget
{
   friend class KonqListViewDir;
   friend class KonqTreeViewDir;

   Q_OBJECT
public:
   KonqTreeViewWidget( KonqListView *parent, QWidget *parentWidget );
   virtual ~KonqTreeViewWidget();

   virtual bool openURL( const KURL &url );

   virtual void saveState( QDataStream &stream );
   virtual void restoreState( QDataStream &stream );

protected slots:
   // slots connected to the directory lister
   virtual void slotCompleted();
   virtual void slotCompleted( const KURL & );
   virtual void slotClear();
   virtual void slotClear( const KURL & );
   virtual void slotRedirection( const KURL &, const KURL & );
   virtual void slotNewItems( const KFileItemList & );
   virtual void slotDeleteItem( KFileItem *_fileTtem );

protected:
   KonqListViewDir *findDir( const QString &_url );

   // Called by KonqListViewDir
   void addSubDir( KonqListViewDir *_dir );
   void removeSubDir( const KURL & _url );
   void clearSubDir( const KURL & _url );

   void openSubFolder( KonqListViewDir *_dir, bool _reload );
   void stopListingSubFolder( KonqListViewDir *_dir );

   // URL -> item (for directories only)
   QDict<KonqListViewDir> m_dictSubDirs;

   QStringList m_urlsToOpen, m_urlsToReload;
};

#endif
