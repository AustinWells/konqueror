/*  This file is part of the KDE project
    Copyright (C) 2000  David Faure <faure@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __konq_operations_h__
#define __konq_operations_h__

#include <kurl.h>
#include <qobject.h>

namespace KIO { class Job; }

/**
 * Implements file operations (move,del,trash,shred,paste...)
 * for konqueror and kdesktop whatever the view mode is (icon, tree, ...)
 */
class KonqOperations : public QObject
{
    Q_OBJECT
protected:
    KonqOperations() {}
    virtual ~KonqOperations() {}

public:
    enum { TRASH, DEL, SHRED };
    static void del( int method, const KURL::List & selectedURLs );
    /**
     * Drop
     * @param dest destination url for the drop (usually background url or item url)
     * @param ev the drop event
     * @param receiver an object that has to have a slotResult( KIO::Job* ) method
     * All views should have that for other jobs they use anyway. Let's re-use !
     */
    static void doDrop( const KURL & dest, QDropEvent * ev, QObject * receiver );

protected:
    bool askDeleteConfirmation( const KURL::List & selectedURLs );
    void _del( int method, const KURL::List & selectedURLs );

protected slots:

    void slotResult( KIO::Job * job );
};

#endif
