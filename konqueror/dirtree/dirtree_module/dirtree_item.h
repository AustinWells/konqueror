/* This file is part of the KDE project
   Copyright (C) 2000 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef dirtree_item_h
#define dirtree_item_h

#include "konq_treeitem.h"
#include <kurl.h>
#include <konq_fileitem.h>
class QDropEvent;

class KonqDirTreeItem : public KonqTreeItem
{
public:
    KonqDirTreeItem( KonqTreeItem *parentItem, KonqTreeTopLevelItem *topLevelItem, KonqFileItem *fileItem );
    KonqDirTreeItem( KonqTree *parent, KonqTreeTopLevelItem *topLevelItem, KonqFileItem *fileItem );
    ~KonqDirTreeItem();

    KonqFileItem *fileItem() const { return m_fileItem; }

    virtual void setOpen( bool open );

    virtual void paintCell( QPainter *_painter, const QColorGroup & _cg, int _column, int _width, int _alignment );

    virtual bool acceptsDrops( const QStrList & formats );
    virtual void drop( QDropEvent * ev );
    virtual QDragObject * dragObject( QWidget * parent, bool move = false );

    virtual void middleButtonPressed();
    virtual void rightButtonPressed();

    virtual void paste();
    virtual void trash();
    virtual void del();
    virtual void shred();

    // The URL to open when this link is clicked
    virtual KURL externalURL() const;
    virtual QString externalMimeType() const;

    virtual void itemSelected();

private:
    void delOperation( int method );
    KonqFileItem *m_fileItem;

};

#endif
