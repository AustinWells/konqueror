/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __kfm_run_h__
#define __kfm_run_h__

#include "krun.h"
#include <sys/types.h>

class KonqMainWindow;
class KonqView;

class KonqRun : public KRun
{
  Q_OBJECT
public:
  /**
   * Create a KonqRun instance, associated to the main view and an
   * optionnal child view.
   */
  KonqRun( KonqMainWindow* mainWindow, KonqView *childView,
           const KURL &url, mode_t _mode = 0,
           bool _is_local_file = false, bool _auto_delete = true );

  virtual ~KonqRun();

  /**
   * Returns true if we found the servicetype for the given url.
   */
  bool foundMimeType() const { return m_bFoundMimeType; }

  KonqView *childView() const { return m_pView; }

protected:
  /**
   * Called if the mimetype has been detected. The function checks wether the document
   * and appends the gzip protocol to the URL. Otherwise @ref #runURL is called to
   * finish the job.
   */
  virtual void foundMimeType( const QString & _type );
  
  virtual void scanFile();

protected slots:
  void slotKonqScanFinished(KIO::Job *job);
  void slotKonqMimetype(KIO::Job *job, const QString &type);

protected:
  KonqMainWindow* m_pMainWindow;
  KonqView* m_pView;
  bool m_bFoundMimeType;
};

#endif
