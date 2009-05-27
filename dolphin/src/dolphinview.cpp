/***************************************************************************
 *   Copyright (C) 2006 by Peter Penz <peter.penz@gmx.at>                  *
 *   Copyright (C) 2006 by Gregor Kališnik <gregor@podnapisi.net>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "dolphinview.h"

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QItemSelection>
#include <QBoxLayout>
#include <QTimer>
#include <QScrollBar>

#include <kactioncollection.h>
#include <kcolorscheme.h>
#include <kdirlister.h>
#include <kfilepreviewgenerator.h>
#include <kiconeffect.h>
#include <kfileitem.h>
#include <klocale.h>
#include <kio/deletejob.h>
#include <kio/netaccess.h>
#include <kio/previewjob.h>
#include <kjob.h>
#include <kmenu.h>
#include <kmessagebox.h>
#include <kmimetyperesolver.h>
#include <konq_fileitemcapabilities.h>
#include <konq_operations.h>
#include <konqmimedata.h>
#include <kstringhandler.h>
#include <ktoggleaction.h>
#include <kurl.h>

#include "dolphinmodel.h"
#include "dolphincolumnview.h"
#include "dolphincontroller.h"
#include "dolphinfileitemdelegate.h"
#include "dolphinsortfilterproxymodel.h"
#include "dolphindetailsview.h"
#include "dolphin_detailsmodesettings.h"
#include "dolphiniconsview.h"
#include "dolphinsettings.h"
#include "dolphin_generalsettings.h"
#include "draganddrophelper.h"
#include "folderexpander.h"
#include "renamedialog.h"
#include "tooltipmanager.h"
#include "viewproperties.h"
#include "zoomlevelinfo.h"

/**
 * Helper function for sorting items with qSort() in
 * DolphinView::renameSelectedItems().
 */
bool lessThan(const KFileItem& item1, const KFileItem& item2)
{
    return KStringHandler::naturalCompare(item1.name(), item2.name()) < 0;
}

DolphinView::DolphinView(QWidget* parent,
                         const KUrl& url,
                         KDirLister* dirLister,
                         DolphinModel* dolphinModel,
                         DolphinSortFilterProxyModel* proxyModel) :
    QWidget(parent),
    m_active(true),
    m_showPreview(false),
    m_loadingDirectory(false),
    m_storedCategorizedSorting(false),
    m_tabsForFiles(false),
    m_isContextMenuOpen(false),
    m_ignoreViewProperties(false),
    m_mode(DolphinView::IconsView),
    m_topLayout(0),
    m_controller(0),
    m_iconsView(0),
    m_detailsView(0),
    m_columnView(0),
    m_fileItemDelegate(0),
    m_selectionModel(0),
    m_dolphinModel(dolphinModel),
    m_dirLister(dirLister),
    m_proxyModel(proxyModel),
    m_previewGenerator(0),
    m_toolTipManager(0),
    m_rootUrl(),
    m_currentItemUrl(),
    m_expandedViews()
{
    m_topLayout = new QVBoxLayout(this);
    m_topLayout->setSpacing(0);
    m_topLayout->setMargin(0);

    m_controller = new DolphinController(this);
    m_controller->setUrl(url);

    connect(m_controller, SIGNAL(urlChanged(const KUrl&)),
            this, SIGNAL(urlChanged(const KUrl&)));
    connect(m_controller, SIGNAL(requestUrlChange(const KUrl&)),
            this, SLOT(slotRequestUrlChange(const KUrl&)));

    connect(m_controller, SIGNAL(requestContextMenu(const QPoint&)),
            this, SLOT(openContextMenu(const QPoint&)));
    connect(m_controller, SIGNAL(urlsDropped(const KFileItem&, const KUrl&, QDropEvent*)),
            this, SLOT(dropUrls(const KFileItem&, const KUrl&, QDropEvent*)));
    connect(m_controller, SIGNAL(sortingChanged(DolphinView::Sorting)),
            this, SLOT(updateSorting(DolphinView::Sorting)));
    connect(m_controller, SIGNAL(sortOrderChanged(Qt::SortOrder)),
            this, SLOT(updateSortOrder(Qt::SortOrder)));
    connect(m_controller, SIGNAL(additionalInfoChanged(const KFileItemDelegate::InformationList&)),
            this, SLOT(updateAdditionalInfo(const KFileItemDelegate::InformationList&)));
    connect(m_controller, SIGNAL(itemTriggered(const KFileItem&)),
            this, SLOT(triggerItem(const KFileItem&)));
    connect(m_controller, SIGNAL(tabRequested(const KUrl&)),
            this, SIGNAL(tabRequested(const KUrl&)));
    connect(m_controller, SIGNAL(activated()),
            this, SLOT(activate()));
    connect(m_controller, SIGNAL(itemEntered(const KFileItem&)),
            this, SLOT(showHoverInformation(const KFileItem&)));
    connect(m_controller, SIGNAL(viewportEntered()),
            this, SLOT(clearHoverInformation()));

    connect(m_dirLister, SIGNAL(redirection(KUrl, KUrl)),
            this, SIGNAL(redirection(KUrl, KUrl)));
    connect(m_dirLister, SIGNAL(completed()),
            this, SLOT(restoreCurrentItem()));

    applyViewProperties(url);
    m_topLayout->addWidget(itemView());
}

DolphinView::~DolphinView()
{
    deleteExpandedViews();
}

const KUrl& DolphinView::url() const
{
    return m_controller->url();
}

KUrl DolphinView::rootUrl() const
{
    return isColumnViewActive() ? m_columnView->rootUrl() : url();
}

void DolphinView::setActive(bool active)
{
    if (active == m_active) {
        return;
    }

    m_active = active;

    QColor color = KColorScheme(QPalette::Active, KColorScheme::View).background().color();
    if (active) {
        // TODO: emitting urlChanged() is a hack, as the URL hasn't really changed. It
        // bypasses the problem when having a split view and changing the active view to
        // update the some URL dependent states. A nicer approach should be no big deal...
        emit urlChanged(url());
        emit selectionChanged(selectedItems());
    } else {
        color.setAlpha(150);
    }

    QWidget* viewport = itemView()->viewport();
    QPalette palette;
    palette.setColor(viewport->backgroundRole(), color);
    viewport->setPalette(palette);

    update();

    if (active) {
        itemView()->setFocus();
        emit activated();
    }

    m_controller->indicateActivationChange(active);
}

bool DolphinView::isActive() const
{
    return m_active;
}

void DolphinView::setMode(Mode mode)
{
    if (mode == m_mode) {
        return; // the wished mode is already set
    }

    const int oldZoomLevel = m_controller->zoomLevel();
    m_mode = mode;

    deleteView();

    const KUrl viewPropsUrl = viewPropertiesUrl();
    ViewProperties props(viewPropsUrl);
    props.setViewMode(m_mode);
    createView();

    // the file item delegate has been recreated, apply the current
    // additional information manually
    const KFileItemDelegate::InformationList infoList = props.additionalInfo();
    m_fileItemDelegate->setShowInformation(infoList);
    emit additionalInfoChanged();

    // Not all view modes support categorized sorting. Adjust the sorting model
    // if changing the view mode results in a change of the categorized sorting
    // capabilities.
    m_storedCategorizedSorting = props.categorizedSorting();
    const bool categorized = m_storedCategorizedSorting && supportsCategorizedSorting();
    if (categorized != m_proxyModel->isCategorizedModel()) {
        m_proxyModel->setCategorizedModel(categorized);
        emit categorizedSortingChanged();
    }

    emit modeChanged();

    updateZoomLevel(oldZoomLevel);
    if (m_showPreview) {
        loadDirectory(viewPropsUrl);
    }
}

DolphinView::Mode DolphinView::mode() const
{
    return m_mode;
}

bool DolphinView::showPreview() const
{
    return m_showPreview;
}

bool DolphinView::showHiddenFiles() const
{
    return m_dirLister->showingDotFiles();
}

bool DolphinView::categorizedSorting() const
{
    // If all view modes would support categorized sorting, returning
    // m_proxyModel->isCategorizedModel() would be the way to go. As
    // currently only the icons view supports caterized sorting, we remember
    // the stored view properties state in m_storedCategorizedSorting and
    // return this state. The application takes care to disable the corresponding
    // checkbox by checking DolphinView::supportsCategorizedSorting() to indicate
    // that this setting is not applied to the current view mode.
    return m_storedCategorizedSorting;
}

bool DolphinView::supportsCategorizedSorting() const
{
    return m_iconsView != 0;
}

void DolphinView::selectAll()
{
    QAbstractItemView* view = itemView();
    // TODO: there seems to be a bug in QAbstractItemView::selectAll(); if
    // the Ctrl-key is pressed (e. g. for Ctrl+A), selectAll() inverts the
    // selection instead of selecting all items. This is bypassed for KDE 4.0
    // by invoking clearSelection() first.
    view->clearSelection();
    view->selectAll();
}

void DolphinView::invertSelection()
{
    if (isColumnViewActive()) {
        // QAbstractItemView does not offer a virtual method invertSelection()
        // as counterpart to QAbstractItemView::selectAll(). This makes it
        // necessary to delegate the inverting of the selection to the
        // column view, as only the selection of the active column should
        // get inverted.
        m_columnView->invertSelection();
    } else {
        QItemSelectionModel* selectionModel = itemView()->selectionModel();
        const QAbstractItemModel* itemModel = selectionModel->model();

        const QModelIndex topLeft = itemModel->index(0, 0);
        const QModelIndex bottomRight = itemModel->index(itemModel->rowCount() - 1,
                                                         itemModel->columnCount() - 1);

        const QItemSelection selection(topLeft, bottomRight);
        selectionModel->select(selection, QItemSelectionModel::Toggle);
    }
}

bool DolphinView::hasSelection() const
{
    return itemView()->selectionModel()->hasSelection();
}

void DolphinView::clearSelection()
{
    itemView()->selectionModel()->clear();
}

KFileItemList DolphinView::selectedItems() const
{
    if (isColumnViewActive()) {
        return m_columnView->selectedItems();
    }

    const QAbstractItemView* view = itemView();

    // Our view has a selection, we will map them back to the DolphinModel
    // and then fill the KFileItemList.
    Q_ASSERT((view != 0) && (view->selectionModel() != 0));

    const QItemSelection selection = m_proxyModel->mapSelectionToSource(view->selectionModel()->selection());
    KFileItemList itemList;

    const QModelIndexList indexList = selection.indexes();
    foreach (const QModelIndex &index, indexList) {
        KFileItem item = m_dolphinModel->itemForIndex(index);
        if (!item.isNull()) {
            itemList.append(item);
        }
    }

    return itemList;
}

KUrl::List DolphinView::selectedUrls() const
{
    KUrl::List urls;
    const KFileItemList list = selectedItems();
    foreach (const KFileItem &item, list) {
        urls.append(item.url());
    }
    return urls;
}

int DolphinView::selectedItemsCount() const
{
    if (isColumnViewActive()) {
        // TODO: get rid of this special case by adjusting the dir lister
        // to the current column
        return m_columnView->selectedItems().count();
    }

    return itemView()->selectionModel()->selection().count();
}

void DolphinView::setContentsPosition(int x, int y)
{
    QAbstractItemView* view = itemView();

    // the ColumnView takes care itself for the horizontal scrolling
    if (!isColumnViewActive()) {
        view->horizontalScrollBar()->setValue(x);
    }
    view->verticalScrollBar()->setValue(y);

    m_loadingDirectory = false;
}

QPoint DolphinView::contentsPosition() const
{
    const int x = itemView()->horizontalScrollBar()->value();
    const int y = itemView()->verticalScrollBar()->value();
    return QPoint(x, y);
}

void DolphinView::setZoomLevel(int level)
{
    if (level < ZoomLevelInfo::minimumLevel()) {
        level = ZoomLevelInfo::minimumLevel();
    } else if (level > ZoomLevelInfo::maximumLevel()) {
        level = ZoomLevelInfo::maximumLevel();
    }

    if (level != zoomLevel()) {
        m_controller->setZoomLevel(level);
        m_previewGenerator->updatePreviews();
        emit zoomLevelChanged(level);
    }
}

int DolphinView::zoomLevel() const
{
    return m_controller->zoomLevel();
}

void DolphinView::setSorting(Sorting sorting)
{
    if (sorting != this->sorting()) {
        updateSorting(sorting);
    }
}

DolphinView::Sorting DolphinView::sorting() const
{
    return m_proxyModel->sorting();
}

void DolphinView::setSortOrder(Qt::SortOrder order)
{
    if (sortOrder() != order) {
        updateSortOrder(order);
    }
}

Qt::SortOrder DolphinView::sortOrder() const
{
    return m_proxyModel->sortOrder();
}

void DolphinView::setAdditionalInfo(KFileItemDelegate::InformationList info)
{
    const KUrl viewPropsUrl = viewPropertiesUrl();
    ViewProperties props(viewPropsUrl);
    props.setAdditionalInfo(info);
    m_fileItemDelegate->setShowInformation(info);

    emit additionalInfoChanged();

    if (itemView() != m_detailsView) {
        // the details view requires no reloading of the directory, as it maps
        // the file item delegate info to its columns internally
        loadDirectory(viewPropsUrl);
    }
}

KFileItemDelegate::InformationList DolphinView::additionalInfo() const
{
    return m_fileItemDelegate->showInformation();
}

void DolphinView::reload()
{
    setUrl(url());
    loadDirectory(url(), true);
}

void DolphinView::refresh()
{
    m_ignoreViewProperties = false;

    const bool oldActivationState = m_active;
    const int oldZoomLevel = m_controller->zoomLevel();
    m_active = true;

    createView();
    applyViewProperties(m_controller->url());
    reload();

    setActive(oldActivationState);
    updateZoomLevel(oldZoomLevel);
}

void DolphinView::updateView(const KUrl& url, const KUrl& rootUrl)
{
    if (m_controller->url() == url) {
        return;
    }

    m_previewGenerator->cancelPreviews();
    m_controller->setUrl(url); // emits urlChanged, which we forward

    if (!rootUrl.isEmpty() && rootUrl.isParentOf(url)) {
        applyViewProperties(rootUrl);
        loadDirectory(rootUrl);
        if (itemView() == m_columnView) {
            m_columnView->setRootUrl(rootUrl);
            m_columnView->showColumn(url);
        }
    } else {
        applyViewProperties(url);
        loadDirectory(url);
    }

    emit startedPathLoading(url);
}

void DolphinView::setNameFilter(const QString& nameFilter)
{
    m_proxyModel->setFilterRegExp(nameFilter);

    if (isColumnViewActive()) {
        // adjusting the directory lister is not enough in the case of the
        // column view, as each column has its own directory lister internally...
        m_columnView->setNameFilter(nameFilter);
    }
}

void DolphinView::calculateItemCount(int& fileCount,
                                     int& folderCount,
                                     KIO::filesize_t& totalFileSize) const
{
    foreach (const KFileItem& item, m_dirLister->items()) {
        if (item.isDir()) {
            ++folderCount;
        } else {
            ++fileCount;
            totalFileSize += item.size();
        }
    }
}

QString DolphinView::statusBarText() const
{
    QString text;
    int folderCount = 0;
    int fileCount = 0;
    KIO::filesize_t totalFileSize = 0;

    if (hasSelection()) {
        // give a summary of the status of the selected files
        const KFileItemList list = selectedItems();
        if (list.isEmpty()) {
            // when an item is triggered, it is temporary selected but selectedItems()
            // will return an empty list
            return text;
        }

        KFileItemList::const_iterator it = list.begin();
        const KFileItemList::const_iterator end = list.end();
        while (it != end) {
            const KFileItem& item = *it;
            if (item.isDir()) {
                ++folderCount;
            } else {
                ++fileCount;
                totalFileSize += item.size();
            }
            ++it;
        }

        if (folderCount + fileCount == 1) {
            // if only one item is selected, show the filename
            const QString name = list.first().name();
            text = (folderCount == 1) ? i18nc("@info:status", "<filename>%1</filename> selected", name) :
                                        i18nc("@info:status", "<filename>%1</filename> selected (%2)",
                                              name, KIO::convertSize(totalFileSize));
        } else {
            // at least 2 items are selected
            const QString foldersText = i18ncp("@info:status", "1 Folder selected", "%1 Folders selected", folderCount);
            const QString filesText = i18ncp("@info:status", "1 File selected", "%1 Files selected", fileCount);
            if ((folderCount > 0) && (fileCount > 0)) {
                text = i18nc("@info:status folders, files (size)", "%1, %2 (%3)",
                             foldersText, filesText, KIO::convertSize(totalFileSize));
            } else if (fileCount > 0) {
                text = i18nc("@info:status files (size)", "%1 (%2)", filesText, KIO::convertSize(totalFileSize));
            } else {
                Q_ASSERT(folderCount > 0);
                text = foldersText;
            }
        }
    } else {
        calculateItemCount(fileCount, folderCount, totalFileSize);
        text = KIO::itemsSummaryString(fileCount + folderCount,
                                       fileCount, folderCount,
                                       totalFileSize, true);
    }

    return text;
}

void DolphinView::setUrl(const KUrl& url)
{
    // remember current item candidate (see restoreCurrentItem())
    m_currentItemUrl = url;
    updateView(url, KUrl());
}

void DolphinView::changeSelection(const KFileItemList& selection)
{
    clearSelection();
    if (selection.isEmpty()) {
        return;
    }
    const KUrl& baseUrl = url();
    KUrl url;
    QItemSelection new_selection;
    foreach(const KFileItem& item, selection) {
        url = item.url().upUrl();
        if (baseUrl.equals(url, KUrl::CompareWithoutTrailingSlash)) {
            QModelIndex index = m_proxyModel->mapFromSource(m_dolphinModel->indexForItem(item));
            new_selection.select(index, index);
        }
    }
    itemView()->selectionModel()->select(new_selection,
                                         QItemSelectionModel::ClearAndSelect
                                         | QItemSelectionModel::Current);
}

void DolphinView::renameSelectedItems()
{
    KFileItemList items = selectedItems();
    const int itemCount = items.count();
    if (itemCount < 1) {
        return;
    }

    if (itemCount > 1) {
        // More than one item has been selected for renaming. Open
        // a rename dialog and rename all items afterwards.
        RenameDialog dialog(this, items);
        if (dialog.exec() == QDialog::Rejected) {
            return;
        }

        const QString newName = dialog.newName();
        if (newName.isEmpty()) {
            emit errorMessage(dialog.errorString());
        } else {
            // TODO: check how this can be integrated into KIO::FileUndoManager/KonqOperations
            // as one operation instead of n rename operations like it is done now...
            Q_ASSERT(newName.contains('#'));

            // currently the items are sorted by the selection order, resort
            // them by the file name
            qSort(items.begin(), items.end(), lessThan);

            // iterate through all selected items and rename them...
            int index = 1;
            foreach (const KFileItem& item, items) {
                const KUrl& oldUrl = item.url();
                QString number;
                number.setNum(index++);

                QString name = newName;
                name.replace('#', number);

                if (oldUrl.fileName() != name) {
                    KUrl newUrl = oldUrl;
                    newUrl.setFileName(name);
                    KonqOperations::rename(this, oldUrl, newUrl);
                }
            }
        }
    } else if (DolphinSettings::instance().generalSettings()->renameInline()) {
        Q_ASSERT(itemCount == 1);

        if (isColumnViewActive()) {
            m_columnView->editItem(items.first());
        } else {
            const QModelIndex dirIndex = m_dolphinModel->indexForItem(items.first());
            const QModelIndex proxyIndex = m_proxyModel->mapFromSource(dirIndex);
            itemView()->edit(proxyIndex);
        }
    } else {
        Q_ASSERT(itemCount == 1);

        RenameDialog dialog(this, items);
        if (dialog.exec() == QDialog::Rejected) {
            return;
        }

        const QString& newName = dialog.newName();
        if (newName.isEmpty()) {
            emit errorMessage(dialog.errorString());
        } else {
            const KUrl& oldUrl = items.first().url();
            KUrl newUrl = oldUrl;
            newUrl.setFileName(newName);
            KonqOperations::rename(this, oldUrl, newUrl);
        }
    }
}

void DolphinView::trashSelectedItems()
{
    const KUrl::List list = simplifiedSelectedUrls();
    KonqOperations::del(this, KonqOperations::TRASH, list);
}

void DolphinView::deleteSelectedItems()
{
    const KUrl::List list = simplifiedSelectedUrls();
    const bool del = KonqOperations::askDeleteConfirmation(list,
                     KonqOperations::DEL,
                     KonqOperations::DEFAULT_CONFIRMATION,
                     this);

    if (del) {
        KIO::Job* job = KIO::del(list);
        connect(job, SIGNAL(result(KJob*)),
                this, SLOT(slotDeleteFileFinished(KJob*)));
    }
}

void DolphinView::cutSelectedItems()
{
    QMimeData* mimeData = selectionMimeData();
    KonqMimeData::addIsCutSelection(mimeData, true);
    QApplication::clipboard()->setMimeData(mimeData);
}

void DolphinView::copySelectedItems()
{
    QMimeData* mimeData = selectionMimeData();
    QApplication::clipboard()->setMimeData(mimeData);
}

void DolphinView::paste()
{
    pasteToUrl(url());
}

void DolphinView::pasteIntoFolder()
{
    const KFileItemList items = selectedItems();
    if ((items.count() == 1) && items.first().isDir()) {
        pasteToUrl(items.first().url());
    }
}

void DolphinView::setShowPreview(bool show)
{
    if (m_showPreview == show) {
        return;
    }

    const KUrl viewPropsUrl = viewPropertiesUrl();
    ViewProperties props(viewPropsUrl);
    props.setShowPreview(show);

    m_showPreview = show;
    m_previewGenerator->setPreviewShown(show);

    const int oldZoomLevel = m_controller->zoomLevel();
    emit showPreviewChanged();

    // Enabling or disabling the preview might change the icon size of the view.
    // As the view does not emit a signal when the icon size has been changed,
    // the used zoom level of the controller must be adjusted manually:
    updateZoomLevel(oldZoomLevel);

    loadDirectory(viewPropsUrl);
}

void DolphinView::setShowHiddenFiles(bool show)
{
    if (m_dirLister->showingDotFiles() == show) {
        return;
    }

    const KUrl viewPropsUrl = viewPropertiesUrl();
    ViewProperties props(viewPropsUrl);
    props.setShowHiddenFiles(show);

    m_dirLister->setShowingDotFiles(show);
    emit showHiddenFilesChanged();

    loadDirectory(viewPropsUrl);
}

void DolphinView::setCategorizedSorting(bool categorized)
{
    if (categorized == categorizedSorting()) {
        return;
    }

    // setCategorizedSorting(true) may only get invoked
    // if the view supports categorized sorting
    Q_ASSERT(!categorized || supportsCategorizedSorting());

    ViewProperties props(viewPropertiesUrl());
    props.setCategorizedSorting(categorized);
    props.save();

    m_storedCategorizedSorting = categorized;
    m_proxyModel->setCategorizedModel(categorized);

    emit categorizedSortingChanged();
}

void DolphinView::toggleSortOrder()
{
    const Qt::SortOrder order = (sortOrder() == Qt::AscendingOrder) ?
                                Qt::DescendingOrder :
                                Qt::AscendingOrder;
    setSortOrder(order);
}

void DolphinView::toggleAdditionalInfo(QAction* action)
{
    const KFileItemDelegate::Information info =
        static_cast<KFileItemDelegate::Information>(action->data().toInt());

    KFileItemDelegate::InformationList list = additionalInfo();

    const bool show = action->isChecked();

    const int index = list.indexOf(info);
    const bool containsInfo = (index >= 0);
    if (show && !containsInfo) {
        list.append(info);
        setAdditionalInfo(list);
    } else if (!show && containsInfo) {
        list.removeAt(index);
        setAdditionalInfo(list);
        Q_ASSERT(list.indexOf(info) < 0);
    }
}

void DolphinView::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
    setActive(true);
}

void DolphinView::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const int delta = event->delta();
        const int level = zoomLevel();
        if (delta > 0) {
            setZoomLevel(level + 1);
        } else if (delta < 0) {
            setZoomLevel(level - 1);
        }
        event->accept();
    }
}

bool DolphinView::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::FocusIn:
        if (watched == itemView()) {
            m_controller->requestActivation();
        }
        break;

    case QEvent::MouseButtonPress:
        if ((watched == itemView()->viewport()) && (m_expandedViews.count() > 0)) {
            // Listening to a mousebutton press event to delete expanded views is a
            // workaround, as it seems impossible for the FolderExpander to know when
            // a dragging outside a view has been finished. However it works quite well:
            // A mousebutton press event indicates that a drag operation must be
            // finished already.
            deleteExpandedViews();
        }
        break;

    case QEvent::DragEnter:
        if (watched == itemView()->viewport()) {
            setActive(true);
        }
        break;

    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

void DolphinView::activate()
{
    setActive(true);
}

void DolphinView::triggerItem(const KFileItem& item)
{
    const Qt::KeyboardModifiers modifier = QApplication::keyboardModifiers();
    if ((modifier & Qt::ShiftModifier) || (modifier & Qt::ControlModifier)) {
        // items are selected by the user, hence don't trigger the
        // item specified by 'index'
        return;
    }

    // TODO: the m_isContextMenuOpen check is a workaround for Qt-issue 207192
    if (item.isNull() || m_isContextMenuOpen) {
        return;
    }

    if (m_toolTipManager != 0) {
        m_toolTipManager->hideTip();
    }
    emit itemTriggered(item); // caught by DolphinViewContainer or DolphinPart
}

void DolphinView::emitSelectionChangedSignal()
{
    emit selectionChanged(DolphinView::selectedItems());
}

void DolphinView::openContextMenu(const QPoint& pos)
{
    KFileItem item;
    if (isColumnViewActive()) {
        item = m_columnView->itemAt(pos);
    } else {
        const QModelIndex index = itemView()->indexAt(pos);
        if (index.isValid() && (index.column() == DolphinModel::Name)) {
            const QModelIndex dolphinModelIndex = m_proxyModel->mapToSource(index);
            item = m_dolphinModel->itemForIndex(dolphinModelIndex);
        }
    }

    if (m_toolTipManager != 0) {
        m_toolTipManager->hideTip();
    }

    m_isContextMenuOpen = true; // TODO: workaround for Qt-issue 207192
    emit requestContextMenu(item, url());
    m_isContextMenuOpen = false;
}

void DolphinView::dropUrls(const KFileItem& destItem,
                           const KUrl& destPath,
                           QDropEvent* event)
{
    DragAndDropHelper::instance().dropUrls(destItem, destPath, event, this);
}

void DolphinView::updateSorting(DolphinView::Sorting sorting)
{
    ViewProperties props(viewPropertiesUrl());
    props.setSorting(sorting);

    m_proxyModel->setSorting(sorting);

    emit sortingChanged(sorting);
}

void DolphinView::updateSortOrder(Qt::SortOrder order)
{
    ViewProperties props(viewPropertiesUrl());
    props.setSortOrder(order);

    m_proxyModel->setSortOrder(order);

    emit sortOrderChanged(order);
}

void DolphinView::updateAdditionalInfo(const KFileItemDelegate::InformationList& info)
{
    ViewProperties props(viewPropertiesUrl());
    props.setAdditionalInfo(info);
    props.save();

    m_fileItemDelegate->setShowInformation(info);

    emit additionalInfoChanged();
}

void DolphinView::updateAdditionalInfoActions(KActionCollection* collection)
{
    const bool enable = (m_mode == DolphinView::DetailsView) ||
                        (m_mode == DolphinView::IconsView);

    QAction* showSizeInfo = collection->action("show_size_info");
    QAction* showDateInfo = collection->action("show_date_info");
    QAction* showPermissionsInfo = collection->action("show_permissions_info");
    QAction* showOwnerInfo = collection->action("show_owner_info");
    QAction* showGroupInfo = collection->action("show_group_info");
    QAction* showMimeInfo = collection->action("show_mime_info");

    showSizeInfo->setChecked(false);
    showDateInfo->setChecked(false);
    showPermissionsInfo->setChecked(false);
    showOwnerInfo->setChecked(false);
    showGroupInfo->setChecked(false);
    showMimeInfo->setChecked(false);

    showSizeInfo->setEnabled(enable);
    showDateInfo->setEnabled(enable);
    showPermissionsInfo->setEnabled(enable);
    showOwnerInfo->setEnabled(enable);
    showGroupInfo->setEnabled(enable);
    showMimeInfo->setEnabled(enable);

    foreach (KFileItemDelegate::Information info, m_fileItemDelegate->showInformation()) {
        switch (info) {
        case KFileItemDelegate::Size:
            showSizeInfo->setChecked(true);
            break;
        case KFileItemDelegate::ModificationTime:
            showDateInfo->setChecked(true);
            break;
        case KFileItemDelegate::Permissions:
            showPermissionsInfo->setChecked(true);
            break;
        case KFileItemDelegate::Owner:
            showOwnerInfo->setChecked(true);
            break;
        case KFileItemDelegate::OwnerAndGroup:
            showGroupInfo->setChecked(true);
            break;
        case KFileItemDelegate::FriendlyMimeType:
            showMimeInfo->setChecked(true);
            break;
        default:
            break;
        }
    }
}

QPair<bool, QString> DolphinView::pasteInfo() const
{
    QPair<bool, QString> ret;
    QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();

    KUrl::List urls = KUrl::List::fromMimeData(mimeData);
    if (!urls.isEmpty()) {
        // disable the paste action if no writing is supported
        KFileItem item(KFileItem::Unknown, KFileItem::Unknown, url());
        ret.first = KonqFileItemCapabilities(KFileItemList() << item).supportsWriting();

        if (urls.count() == 1) {
            const KFileItem item(KFileItem::Unknown, KFileItem::Unknown, urls.first(), true);
            ret.second = item.isDir() ? i18nc("@action:inmenu", "Paste One Folder") :
                                        i18nc("@action:inmenu", "Paste One File");

        } else {
            ret.second = i18ncp("@action:inmenu", "Paste One Item", "Paste %1 Items", urls.count());
        }
    } else {
        ret.first = false;
        ret.second = i18nc("@action:inmenu", "Paste");
    }

    return ret;
}

void DolphinView::setTabsForFilesEnabled(bool tabsForFiles)
{
    m_tabsForFiles = tabsForFiles;
}

bool DolphinView::isTabsForFilesEnabled() const
{
    return m_tabsForFiles;
}

bool DolphinView::itemsExpandable() const
{
    return (m_detailsView != 0) && m_detailsView->itemsExpandable();
}

void DolphinView::emitContentsMoved()
{
    // only emit the contents moved signal if:
    // - no directory loading is ongoing (this would reset the contents position
    //   always to (0, 0))
    // - if the Column View is active: the column view does an automatic
    //   positioning during the loading operation, which must be remembered
    if (!m_loadingDirectory || isColumnViewActive()) {
        const QPoint pos(contentsPosition());
        emit contentsMoved(pos.x(), pos.y());
    }
}

void DolphinView::showHoverInformation(const KFileItem& item)
{
    emit requestItemInfo(item);
}

void DolphinView::clearHoverInformation()
{
    emit requestItemInfo(KFileItem());
}

void DolphinView::slotDeleteFileFinished(KJob* job)
{
    if (job->error() == 0) {
        emit operationCompletedMessage(i18nc("@info:status", "Delete operation completed."));
    } else if (job->error() != KIO::ERR_USER_CANCELED) {
        emit errorMessage(job->errorString());
    }
}

void DolphinView::slotRequestUrlChange(const KUrl& url)
{
    emit requestUrlChange(url);
    m_controller->setUrl(url);
}

void DolphinView::restoreCurrentItem()
{
    if (!m_currentItemUrl.isEmpty()) {
        const QModelIndex dirIndex = m_dolphinModel->indexForUrl(m_currentItemUrl);
        if (dirIndex.isValid()) {
            const QModelIndex proxyIndex = m_proxyModel->mapFromSource(dirIndex);
            QAbstractItemView* view = itemView();
            const bool clearSelection = !hasSelection();
            view->setCurrentIndex(proxyIndex);
            if (clearSelection) {
                view->clearSelection();
            }
        }
        m_currentItemUrl.clear();
    }
}
void DolphinView::enterDir(const QModelIndex& index, QAbstractItemView* view)
{
    // Deleting a view that is the root of a drag operation is not allowed, otherwise
    // the dragging gets automatically cancelled by Qt. So before entering a new
    // directory, the current view is remembered in m_expandedViews and deleted
    // later when the drag operation has been finished (see DolphinView::eventFilter()).
    m_expandedViews.append(view);
    m_controller->triggerItem(index);
}

void DolphinView::loadDirectory(const KUrl& url, bool reload)
{
    if (!url.isValid()) {
        const QString location(url.pathOrUrl());
        if (location.isEmpty()) {
            emit errorMessage(i18nc("@info:status", "The location is empty."));
        } else {
            emit errorMessage(i18nc("@info:status", "The location '%1' is invalid.", location));
        }
        return;
    }

    m_loadingDirectory = true;

    m_dirLister->stop();
    m_dirLister->openUrl(url, reload ? KDirLister::Reload : KDirLister::NoFlags);

    if (isColumnViewActive()) {
        // adjusting the directory lister is not enough in the case of the
        // column view, as each column has its own directory lister internally...
        if (reload) {
            m_columnView->reload();
        } else {
            m_columnView->showColumn(url);
        }
    }
}

KUrl DolphinView::viewPropertiesUrl() const
{
    if (isColumnViewActive()) {
        return m_columnView->rootUrl();
    }

    return url();
}

void DolphinView::applyViewProperties(const KUrl& url)
{
    if (m_ignoreViewProperties) {
        return;
    }

    if (isColumnViewActive() && rootUrl().isParentOf(url)) {
        // The column view is active, hence don't apply the view properties
        // of sub directories (represented by columns) to the view. The
        // view always represents the properties of the first column.
        return;
    }

    const ViewProperties props(url);

    const Mode mode = props.viewMode();
    if (m_mode != mode) {
        const int oldZoomLevel = m_controller->zoomLevel();

        m_mode = mode;
        createView();
        emit modeChanged();

        updateZoomLevel(oldZoomLevel);
    }
    if (itemView() == 0) {
        createView();
    }
    Q_ASSERT(itemView() != 0);
    Q_ASSERT(m_fileItemDelegate != 0);

    const bool showHiddenFiles = props.showHiddenFiles();
    if (showHiddenFiles != m_dirLister->showingDotFiles()) {
        m_dirLister->setShowingDotFiles(showHiddenFiles);
        emit showHiddenFilesChanged();
    }

    m_storedCategorizedSorting = props.categorizedSorting();
    const bool categorized = m_storedCategorizedSorting && supportsCategorizedSorting();
    if (categorized != m_proxyModel->isCategorizedModel()) {
        m_proxyModel->setCategorizedModel(categorized);
        emit categorizedSortingChanged();
    }

    const DolphinView::Sorting sorting = props.sorting();
    if (sorting != m_proxyModel->sorting()) {
        m_proxyModel->setSorting(sorting);
        emit sortingChanged(sorting);
    }

    const Qt::SortOrder sortOrder = props.sortOrder();
    if (sortOrder != m_proxyModel->sortOrder()) {
        m_proxyModel->setSortOrder(sortOrder);
        emit sortOrderChanged(sortOrder);
    }

    KFileItemDelegate::InformationList info = props.additionalInfo();
    if (info != m_fileItemDelegate->showInformation()) {
        m_fileItemDelegate->setShowInformation(info);
        emit additionalInfoChanged();
    }

    const bool showPreview = props.showPreview();
    if (showPreview != m_showPreview) {
        m_showPreview = showPreview;
        m_previewGenerator->setPreviewShown(showPreview);

        const int oldZoomLevel = m_controller->zoomLevel();
        emit showPreviewChanged();

        // Enabling or disabling the preview might change the icon size of the view.
        // As the view does not emit a signal when the icon size has been changed,
        // the used zoom level of the controller must be adjusted manually:
        updateZoomLevel(oldZoomLevel);
    }

    if (DolphinSettings::instance().generalSettings()->globalViewProps()) {
        // During the lifetime of a DolphinView instance the global view properties
        // should not be changed. This allows e. g. to split a view and use different
        // view properties for each view.
        m_ignoreViewProperties = true;
    }
}

void DolphinView::createView()
{
    deleteView();
    Q_ASSERT(m_iconsView == 0);
    Q_ASSERT(m_detailsView == 0);
    Q_ASSERT(m_columnView == 0);

    QAbstractItemView* view = 0;
    switch (m_mode) {
    case IconsView: {
        m_iconsView = new DolphinIconsView(this, m_controller);
        view = m_iconsView;
        break;
    }

    case DetailsView:
        m_detailsView = new DolphinDetailsView(this, m_controller);
        view = m_detailsView;
        break;

    case ColumnView:
        m_columnView = new DolphinColumnView(this, m_controller);
        view = m_columnView;
        break;
    }

    Q_ASSERT(view != 0);
    view->installEventFilter(this);
    view->viewport()->installEventFilter(this);
    setFocusProxy(view);

    if (m_mode != ColumnView) {
        // Give the view the ability to auto-expand its directories on hovering
        // (the column view takes care about this itself). If the details view
        // uses expandable folders, the auto-expanding should be used always.
        DolphinSettings& settings = DolphinSettings::instance();
        const bool enabled = settings.generalSettings()->autoExpandFolders() ||
                            ((m_detailsView != 0) && settings.detailsModeSettings()->expandableFolders());

        FolderExpander* folderExpander = new FolderExpander(view, m_proxyModel);
        folderExpander->setEnabled(enabled);
        connect(folderExpander, SIGNAL(enterDir(const QModelIndex&, QAbstractItemView*)),
                this, SLOT(enterDir(const QModelIndex&, QAbstractItemView*)));
    }

    m_controller->setItemView(view);

    m_fileItemDelegate = new DolphinFileItemDelegate(view);
    m_fileItemDelegate->setShowToolTipWhenElided(false);
    m_fileItemDelegate->setMinimizedNameColumn(m_mode == DetailsView);
    view->setItemDelegate(m_fileItemDelegate);

    view->setModel(m_proxyModel);
    if (m_selectionModel != 0) {
        view->setSelectionModel(m_selectionModel);
    } else {
        m_selectionModel = view->selectionModel();
    }

    // reparent the selection model, as it should not be deleted
    // when deleting the model
    m_selectionModel->setParent(this);

    view->setSelectionMode(QAbstractItemView::ExtendedSelection);

    m_previewGenerator = new KFilePreviewGenerator(view);
    m_previewGenerator->setPreviewShown(m_showPreview);

    if (DolphinSettings::instance().generalSettings()->showToolTips()) {
        m_toolTipManager = new ToolTipManager(view, m_proxyModel);
        connect(m_controller, SIGNAL(hideToolTip()),
                m_toolTipManager, SLOT(hideTip()));
    }

    m_topLayout->insertWidget(1, view);

    connect(view->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this, SLOT(emitSelectionChangedSignal()));
    connect(view->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(emitContentsMoved()));
    connect(view->horizontalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(emitContentsMoved()));
}

void DolphinView::deleteView()
{
    QAbstractItemView* view = itemView();
    if (view != 0) {
        // It's important to set the keyboard focus to the parent
        // before deleting the view: Otherwise when having a split
        // view the other view will get the focus and will request
        // an activation (see DolphinView::eventFilter()).
        setFocusProxy(0);
        setFocus();

        m_topLayout->removeWidget(view);
        view->close();

        disconnect(view);
        m_controller->disconnect(view);
        view->disconnect();

        bool deleteView = true;
        foreach (const QAbstractItemView* expandedView, m_expandedViews) {
            if (view == expandedView) {
                // the current view got already expanded and must stay alive
                // until the dragging has been completed
                deleteView = false;
                break;
            }
        }
        if (deleteView) {
            view->deleteLater();
        }
        view = 0;

        m_iconsView = 0;
        m_detailsView = 0;
        m_columnView = 0;
        m_fileItemDelegate = 0;
        m_previewGenerator = 0;
        m_toolTipManager = 0;
    }
}

QAbstractItemView* DolphinView::itemView() const
{
    if (m_detailsView != 0) {
        return m_detailsView;
    } else if (m_columnView != 0) {
        return m_columnView;
    }

    return m_iconsView;
}

bool DolphinView::isCutItem(const KFileItem& item) const
{
    const QMimeData* mimeData = QApplication::clipboard()->mimeData();
    const KUrl::List cutUrls = KUrl::List::fromMimeData(mimeData);

    const KUrl& itemUrl = item.url();
    KUrl::List::const_iterator it = cutUrls.begin();
    const KUrl::List::const_iterator end = cutUrls.end();
    while (it != end) {
        if (*it == itemUrl) {
            return true;
        }
        ++it;
    }

    return false;
}

void DolphinView::pasteToUrl(const KUrl& url)
{
    QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();

#ifdef KURL_HAS_DECODEOPTIONS
    // Prefer local urls if possible, to avoid problems with desktop:/ urls from other users (#184403)
    const KUrl::List sourceUrls = KUrl::List::fromMimeData(mimeData, KUrl::List::PreferLocalUrls);
#else
    const KUrl::List sourceUrls = KUrl::List::fromMimeData(mimeData);
#endif
    if (KonqMimeData::decodeIsCutSelection(mimeData)) {
        KonqOperations::copy(this, KonqOperations::MOVE, sourceUrls, url);
        clipboard->clear();
    } else {
        KonqOperations::copy(this, KonqOperations::COPY, sourceUrls, url);
    }
}

void DolphinView::updateZoomLevel(int oldZoomLevel)
{
    const int newZoomLevel = ZoomLevelInfo::zoomLevelForIconSize(itemView()->iconSize());
    if (oldZoomLevel != newZoomLevel) {
        m_controller->setZoomLevel(newZoomLevel);
        emit zoomLevelChanged(newZoomLevel);
    }
}

KUrl::List DolphinView::simplifiedSelectedUrls() const
{
    KUrl::List list = selectedUrls();
    if (itemsExpandable() ) {
        list = KDirModel::simplifiedUrlList(list);
    }
    return list;
}

void DolphinView::deleteExpandedViews()
{
    const QAbstractItemView* view = itemView();
    foreach (QAbstractItemView* expandedView, m_expandedViews) {
        if (expandedView != view) {
            expandedView->deleteLater();
        }
    }
    m_expandedViews.clear();
}

QMimeData* DolphinView::selectionMimeData() const
{
    if (isColumnViewActive()) {
        return m_columnView->selectionMimeData();
    }

    const QAbstractItemView* view = itemView();
    Q_ASSERT((view != 0) && (view->selectionModel() != 0));
    const QItemSelection selection = m_proxyModel->mapSelectionToSource(view->selectionModel()->selection());
    return m_dolphinModel->mimeData(selection.indexes());
}

#include "dolphinview.moc"