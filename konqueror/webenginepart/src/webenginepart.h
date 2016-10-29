/*
 * This file is part of the KDE project.
 *
 * Copyright (C) 2007 Trolltech ASA
 * Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
 * Copyright (C) 2009 Dawit Alemayehu <adawit@kde.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef WEBENGINEPART_H
#define WEBENGINEPART_H

#include "kwebenginepartlib_export.h"

#include <QtWebEngineWidgets/QWebEnginePage>

#include <KParts/ReadOnlyPart>
#include <QUrl>

namespace KParts {
  class BrowserExtension;
  class StatusBarExtension;
}

class QWebEngineView;
class QWebEngineHistoryItem;
class WebView;
class WebPage;
class SearchBar;
class PasswordBar;
class FeaturePermissionBar;
class KUrlLabel;
class WebEngineBrowserExtension;

/**
 * A KPart wrapper for the QtWebEngine's browser rendering engine.
 *
 * This class attempts to provide the same type of integration into KPart
 * plugin applications, such as Konqueror, in much the same way as KHTML.
 *
 * Unlink the KHTML part however, access into the internals of the rendering
 * engine are provided through existing QtWebEngine class ; @see QWebEngineView.
 *
 */
class KWEBENGINEPARTLIB_EXPORT WebEnginePart : public KParts::ReadOnlyPart
{
    Q_OBJECT
    Q_PROPERTY( bool modified READ isModified )
public:
    explicit WebEnginePart(QWidget* parentWidget = 0, QObject* parent = Q_NULLPTR,
                         const QByteArray& cachedHistory = QByteArray(),
                         const QStringList& = QStringList());
    ~WebEnginePart();

    /**
     * Re-implemented for internal reasons. API remains unaffected.
     *
     * @see KParts::ReadOnlyPart::openUrl
     */
    virtual bool openUrl(const QUrl &);

    /**
     * Re-implemented for internal reasons. API remains unaffected.
     *
     * @see KParts::ReadOnlyPart::closeUrl
     */
    virtual bool closeUrl();

    /**
     * Returns a pointer to the render widget used to display a web page.
     *
     * @see QWebView.
     */
    virtual QWebEngineView *view();

    /**
     * Checks whether the page contains unsubmitted form changes.
     *
     * @return @p true if form changes exist.
     */
    bool isModified() const;

    /**
     * Connects the appropriate signals from the given page to the slots
     * in this class.
     */
    void connectWebPageSignals(WebPage* page);

    void slotShowFeaturePermissionBar(QWebEnginePage::Feature);
protected:
    /**
     * Re-implemented for internal reasons. API remains unaffected.
     *
     * @see KParts::ReadOnlyPart::guiActivateEvent
     */
    virtual void guiActivateEvent(KParts::GUIActivateEvent *);

    /**
     * Re-implemented for internal reasons. API remains unaffected.
     *
     * @see KParts::ReadOnlyPart::openFile
     */
    virtual bool openFile();

private Q_SLOTS:
    void slotShowSecurity();
    void slotShowSearchBar();
    void slotLoadStarted();
    void slotLoadAborted(const QUrl &);
    void slotLoadFinished(bool);
    void slotMainFrameLoadFinished(bool);

    void slotSearchForText(const QString &text, bool backward);
    void slotLinkHovered(const QString &);
    //void slotSaveFrameState(QWebFrame *frame, QWebHistoryItem *item);
    //void slotRestoreFrameState(QWebFrame *frame);
    void slotLinkMiddleOrCtrlClicked(const QUrl&);
    void slotSelectionClipboardUrlPasted(const QUrl&, const QString&);

    void slotUrlChanged(const QUrl &);
    void slotWalletClosed();
    void slotShowWalletMenu();
    void slotLaunchWalletManager();
    void slotDeleteNonPasswordStorableSite();
    void slotRemoveCachedPasswords();
    void slotSetTextEncoding(QTextCodec*);
    void slotSetStatusBarText(const QString& text);
    void slotWindowCloseRequested();
    void slotSaveFormDataRequested(const QString &, const QUrl &);
    void slotSaveFormDataDone();
    void slotFillFormRequestCompleted(bool);

    void slotFeaturePermissionGranted(QWebEnginePage::Feature);
    void slotFeaturePermissionDenied(QWebEnginePage::Feature);

private:
    WebPage* page();
    const WebPage* page() const;
    void initActions();
    void updateActions();
    void addWalletStatusBarIcon();

    bool m_emitOpenUrlNotify;
    bool m_hasCachedFormData;
    bool m_doLoadFinishedActions;
    KUrlLabel* m_statusBarWalletLabel;
    SearchBar* m_searchBar;
    PasswordBar* m_passwordBar;
    FeaturePermissionBar* m_featurePermissionBar;
    WebEngineBrowserExtension* m_browserExtension;
    KParts::StatusBarExtension* m_statusBarExtension;
    WebView* m_webView;
};

#endif // WEBENGINEPART_H
