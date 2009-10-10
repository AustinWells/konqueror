/***************************************************************************
 *   Copyright (C) 2008 by Sebastian Trueg <trueg@kde.org>                 *
 *   Copyright (C) 2009 by Peter Penz <peter.penz@gmx.at>                  *
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

#include "metadatawidget.h"

#include <kfileitem.h>
#include <klocale.h>

#include <QGridLayout>
#include <QLabel>
#include <QList>
#include <QString>

#include <config-nepomuk.h>
#ifdef HAVE_NEPOMUK
    #define DISABLE_NEPOMUK_LEGACY

    #include "commentwidget_p.h"
    #include "nepomukmassupdatejob_p.h"
    #include "taggingwidget_p.h"

    #include <kconfig.h>
    #include <kconfiggroup.h>

    #include <Nepomuk/KRatingWidget>
    #include <Nepomuk/Resource>
    #include <Nepomuk/Tag>
    #include <Nepomuk/Types/Property>
    #include <Nepomuk/Variant>

    #include <Soprano/Vocabulary/Xesam>
    #include <QMutex>
    #include <QThread>
#endif

class MetaDataWidget::Private
{
public:
    struct Row
    {
        QLabel* label;
        QWidget* infoWidget;
    };

    Private(MetaDataWidget* parent);
    ~Private();

    void addRow(QLabel* label, QWidget* infoWidget);
    void removeMetaInfoRows();
    void setRowVisible(QWidget* infoWidget, bool visible);

    void slotLoadingFinished();

    QList<Row> m_rows;

    QGridLayout* m_gridLayout;

    QLabel* m_typeInfo;
    QLabel* m_sizeLabel;
    QLabel* m_sizeInfo;
    QLabel* m_modifiedInfo;
    QLabel* m_ownerInfo;
    QLabel* m_permissionsInfo;

#ifdef HAVE_NEPOMUK
    KRatingWidget* m_ratingWidget;
    TaggingWidget* m_taggingWidget;
    CommentWidget* m_commentWidget;

    // shared data between the GUI-thread and
    // the loader-thread (see LoadFilesThread):
    QMutex m_mutex;
    struct SharedData
    {
        int rating;
        QString comment;
        QList<Nepomuk::Tag> tags;
        QList<QString> metaInfoLabels;
        QList<QString> metaInfoValues;
    } m_sharedData;

    /**
     * Loads the meta data of files and writes
     * the result into a shared data pool that
     * can be used by the widgets in the GUI thread.
     */
    class LoadFilesThread : public QThread
    {
    public:
        LoadFilesThread(SharedData* m_sharedData, QMutex* m_mutex);
        virtual ~LoadFilesThread();
        void loadFiles(const KUrl::List& urls);
        virtual void run();

    private:
        /**
         * Assures that the settings for the meta information
         * are initialized with proper default values.
         */
        void initMetaInfoSettings(KConfigGroup& group);

        /**
         * Temporary helper method for KDE 4.3 as we currently don't get
         * translated labels for Nepmok literals: Replaces camelcase labels
         * like "fileLocation" by "File Location:".
         */
        QString tunedLabel(const QString& label) const;

    private:
        SharedData* m_m_sharedData;
        QMutex* m_m_mutex;
        KUrl::List m_urls;
        bool m_canceled;
    };

    LoadFilesThread* m_loadFilesThread;
#endif

private:
    MetaDataWidget* const q;
};

MetaDataWidget::Private::Private(MetaDataWidget* parent) :
    m_rows(),
    m_gridLayout(0),
    m_typeInfo(0),
    m_sizeLabel(0),
    m_sizeInfo(0),
    m_modifiedInfo(0),
    m_ownerInfo(0),
    m_permissionsInfo(0),
#ifdef HAVE_NEPOMUK
    m_ratingWidget(0),
    m_taggingWidget(0),
    m_commentWidget(0),
    m_loadFilesThread(0),
#endif
    q(parent)
{
    m_gridLayout = new QGridLayout(parent);

    m_typeInfo = new QLabel(parent);
    m_sizeLabel = new QLabel(parent);
    m_sizeInfo = new QLabel(parent);
    m_modifiedInfo = new QLabel(parent);
    m_ownerInfo = new QLabel(parent);
    m_permissionsInfo = new QLabel(parent);
#ifdef HAVE_NEPOMUK
    m_ratingWidget = new KRatingWidget(parent);
    m_taggingWidget = new TaggingWidget(parent);
    m_commentWidget = new CommentWidget(parent);
#endif

    addRow(new QLabel(i18nc("@label", "Type:"), parent), m_typeInfo);
    addRow(m_sizeLabel, m_sizeInfo);
    addRow(new QLabel(i18nc("@label", "Modified:"), parent), m_modifiedInfo);
    addRow(new QLabel(i18nc("@label", "Owner:"), parent), m_ownerInfo);
    addRow(new QLabel(i18nc("@label", "Permissions:"), parent), m_permissionsInfo);
#ifdef HAVE_NEPOMUK
    addRow(new QLabel(i18nc("@label", "Rating:"), parent), m_ratingWidget);
    addRow(new QLabel(i18nc("@label", "Tags:"), parent), m_taggingWidget);
    addRow(new QLabel(i18nc("@label", "Comment:"), parent), m_commentWidget);

    m_sharedData.rating = 0;
    m_loadFilesThread = new LoadFilesThread(&m_sharedData, &m_mutex);
    connect(m_loadFilesThread, SIGNAL(finished()), q, SLOT(slotLoadingFinished()));
#endif
}

MetaDataWidget::Private::~Private()
{
#ifdef HAVE_NEPOMUK
    delete m_loadFilesThread;
#endif
}

void MetaDataWidget::Private::addRow(QLabel* label, QWidget* infoWidget)
{
    Row row;
    row.label = label;
    row.infoWidget = infoWidget;
    m_rows.append(row);

    // use a brighter color for the label
    QPalette palette = label->palette();
    QColor textColor = palette.color(QPalette::Text);
    textColor.setAlpha(128);
    palette.setColor(QPalette::WindowText, textColor);
    label->setPalette(palette);

    // add the row to grid layout
    const int rowIndex = m_rows.count();
    m_gridLayout->addWidget(label, rowIndex, 0, Qt::AlignLeft);
    m_gridLayout->addWidget(infoWidget, rowIndex, 1, Qt::AlignRight);
}

void MetaDataWidget::Private::setRowVisible(QWidget* infoWidget, bool visible)
{
    foreach (const Row& row, m_rows) {
        if (row.infoWidget == infoWidget) {
            row.label->setVisible(visible);
            row.infoWidget->setVisible(visible);
            return;
        }
    }
}

void MetaDataWidget::Private::slotLoadingFinished()
{
#ifdef HAVE_NEPOMUK
    QMutexLocker locker(&m_mutex);
    m_ratingWidget->setRating(m_sharedData.rating);
    m_commentWidget->setText(m_sharedData.comment);
    m_taggingWidget->setTags(m_sharedData.tags);

    // Show the remaining meta information as text. The number
    // of required rows may very. Existing rows are reused to
    // prevent flickering.
    int index = 8;  // TODO: don't hardcode this value here
    const int rowCount = m_rows.count();
    Q_ASSERT(rowCount >= index);

    Q_ASSERT(m_sharedData.metaInfoLabels.count() == m_sharedData.metaInfoValues.count());
    const int metaInfoCount = m_sharedData.metaInfoLabels.count();
    for (int i = 0; i < metaInfoCount; ++i) {
        if (index < rowCount) {
            // adjust texts of the current row
            m_rows[index].label->setText(m_sharedData.metaInfoLabels[i]);
            QLabel* infoValueLabel = qobject_cast<QLabel*>(m_rows[index].infoWidget);
            Q_ASSERT(infoValueLabel != 0);
            infoValueLabel->setText(m_sharedData.metaInfoValues[i]);
        } else {
            // create new row
            QLabel* infoLabel = new QLabel(m_sharedData.metaInfoLabels[i], q);
            QLabel* infoValue = new QLabel(m_sharedData.metaInfoValues[i], q);
            addRow(infoLabel, infoValue);
        }
        ++index;
    }

    // remove rows that are not needed anymore
    for (int i = rowCount - 1; i > index; --i) {
        delete m_rows[i].label;
        delete m_rows[i].infoWidget;
        m_rows.pop_back();
    }
#endif
}

#ifdef HAVE_NEPOMUK
MetaDataWidget::Private::LoadFilesThread::LoadFilesThread(
                            MetaDataWidget::Private::SharedData* m_sharedData,
                            QMutex* m_mutex) :
    m_m_sharedData(m_sharedData),
    m_m_mutex(m_mutex),
    m_urls(),
    m_canceled(false)
{
}

MetaDataWidget::Private::LoadFilesThread::~LoadFilesThread()
{
    // This thread may very well be deleted during execution. We need
    // to protect it from crashes here.
    m_canceled = true;
    wait();
}

void MetaDataWidget::Private::LoadFilesThread::loadFiles(const KUrl::List& urls)
{
    QMutexLocker locker(m_m_mutex);
    m_urls = urls;
    m_canceled = false;
    start();
}

void MetaDataWidget::Private::LoadFilesThread::run()
{
    QMutexLocker locker(m_m_mutex);
    const KUrl::List urls = m_urls;
    locker.unlock();

    KConfig config("kmetainformationrc", KConfig::NoGlobals);
    KConfigGroup settings = config.group("Show");
    initMetaInfoSettings(settings);

    bool first = true;
    unsigned int rating = 0;
    QString comment;
    QList<Nepomuk::Tag> tags;
    QList<QString> metaInfoLabels;
    QList<QString> metaInfoValues;
    foreach (const KUrl& url, urls) {
        if (m_canceled) {
            return;
        }

        Nepomuk::Resource file(url, Soprano::Vocabulary::Xesam::File());

        if (!first && (rating != file.rating())) {
            rating = 0; // reset rating
        } else if (first) {
            rating = file.rating();
        }

        if (!first && (comment != file.description())) {
            comment.clear(); // reset comment
        } else if (first) {
            comment = file.description();
        }

        if (!first && (tags != file.tags())) {
            tags.clear(); // reset tags
        } else if (first) {
            tags = file.tags();
        }

        if (first && (urls.count() == 1)) {
            // TODO: show shared meta informations instead
            // of not showing anything on multiple selections
            QHash<QUrl, Nepomuk::Variant> properties = file.properties();
            QHash<QUrl, Nepomuk::Variant>::const_iterator it = properties.constBegin();
            while (it != properties.constEnd()) {
                Nepomuk::Types::Property prop(it.key());
                if (true /*settings.readEntry(prop.name(), true)*/) {
                    // TODO #1: use Nepomuk::formatValue(res, prop) if available
                    // instead of it.value().toString()
                    // TODO #2: using tunedLabel() is a workaround for KDE 4.3 until
                    // we get translated labels
                    metaInfoLabels.append(tunedLabel(prop.label()));
                    metaInfoValues.append(it.value().toString());
                }
                ++it;
            }
        }

        first = false;
    }

    locker.relock();
    m_m_sharedData->rating = rating;
    m_m_sharedData->comment = comment;
    m_m_sharedData->tags = tags;
    m_m_sharedData->metaInfoLabels = metaInfoLabels;
    m_m_sharedData->metaInfoValues = metaInfoValues;
}

void MetaDataWidget::Private::LoadFilesThread::initMetaInfoSettings(KConfigGroup& group)
{
    if (!group.readEntry("initialized", false)) {
        // The resource file is read the first time. Assure
        // that some meta information is disabled per default.

        static const char* disabledProperties[] = {
            "asText", "contentSize", "depth", "fileExtension",
            "fileName", "fileSize", "isPartOf", "mimetype", "name",
            "parentUrl", "plainTextContent", "sourceModified",
            "size", "url",
            0 // mandatory last entry
        };

        int i = 0;
        while (disabledProperties[i] != 0) {
            group.writeEntry(disabledProperties[i], false);
            ++i;
        }

        // mark the group as initialized
        group.writeEntry("initialized", true);
    }
}

QString MetaDataWidget::Private::LoadFilesThread::tunedLabel(const QString& label) const
{
    QString tunedLabel;
    const int labelLength = label.length();
    if (labelLength > 0) {
        tunedLabel.reserve(labelLength);
        tunedLabel = label[0].toUpper();
        for (int i = 1; i < labelLength; ++i) {
            if (label[i].isUpper() && !label[i - 1].isSpace() && !label[i - 1].isUpper()) {
                tunedLabel += ' ';
                tunedLabel += label[i].toLower();
            } else {
                tunedLabel += label[i];
            }
        }
    }
    return tunedLabel + ':';
}

#endif

MetaDataWidget::MetaDataWidget(QWidget* parent) :
    QWidget(parent),
    d(new Private(this))
{
}

MetaDataWidget::~MetaDataWidget()
{
    delete d;
}

void MetaDataWidget::setItem(const KFileItem& item)
{
    // update values for "type", "size", "modified",
    // "owner" and "permissions" synchronously
    d->m_sizeLabel->setText(i18nc("@label", "Size:"));
    if (item.isDir()) {
        d->m_typeInfo->setText(i18nc("@label", "Folder"));
        d->setRowVisible(d->m_sizeInfo, false);
    } else {
        d->m_typeInfo->setText(item.mimeComment());
        d->m_sizeInfo->setText(KIO::convertSize(item.size()));
        d->setRowVisible(d->m_sizeInfo, true);
    }
    d->m_modifiedInfo->setText(item.timeString());
    d->m_ownerInfo->setText(item.user());
    d->m_permissionsInfo->setText(item.permissionsString());

    setItems(KFileItemList() << item);
}

void MetaDataWidget::setItems(const KFileItemList& items)
{
    if (items.count() > 1) {
        // calculate the size of all items and show this
        // information to the user
        d->m_sizeLabel->setText(i18nc("@label", "Total Size:"));
        d->setRowVisible(d->m_sizeInfo, true);

        quint64 totalSize = 0;
        foreach (const KFileItem& item, items) {
            if (!item.isDir() && !item.isLink()) {
                totalSize += item.size();
            }
        }
        d->m_sizeInfo->setText(KIO::convertSize(totalSize));
    }

#ifdef HAVE_NEPOMUK
    QList<KUrl> urls;
    foreach (const KFileItem& item, items) {
        const KUrl url = item.nepomukUri();
        if (url.isValid()) {
            urls.append(url);
        }
    }
    d->m_loadFilesThread->loadFiles(urls);
#endif
}

#include "metadatawidget.moc"