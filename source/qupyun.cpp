#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>

#include "qupyun.h"

static const char SEPARATOR = '/';
static const QLatin1String &MKDIR = QLatin1String("folder");
static const char * const SDK_VERSION = "1.0";

enum API
{
    BucketUsage,
    Mkdir,
    Rmdir,
    Ls,
    Upload,
    Read,
    RemoveFile,
    FileProp
};

class QUpYun::Private : public QObject
{
    Q_OBJECT
public:
    Private(QUpYun *upyun) :
        QObject(upyun),
        q(upyun),
        manager(new QNetworkAccessManager(this)),
        apiDomain(QUpYun::ED_AUTO)
    {
        connect(manager, SIGNAL(finished(QNetworkReply*)),
                this, SLOT(requestFinished(QNetworkReply*)));
    }

    ~Private()
    {
    }

    inline QString upyunAPIDomain() const;

    QNetworkReply *sendRequest(QNetworkAccessManager::Operation method,
                               const QString &uri,
                               const QByteArray &data = QByteArray(),
                               bool autoMkdir = false,
                               const RequestParams &params = RequestParams());

    inline QString formatPath(const QString &path) const;
    inline QByteArray md5(const QByteArray &data) const;
    inline QByteArray getGMTDate() const;
    inline QByteArray signature(QNetworkAccessManager::Operation method,
                                const QString &date,
                                const QString &uri,
                                qlonglong length) const;

    QUpYun *q;
    QNetworkAccessManager *manager;
    QHash<QNetworkReply *, API> requests;

    QString bucketName; // Bucket name.
    QString userName;   // User name.
    QString password;   // User password after MD5.

    QUpYun::EndPoint apiDomain; // API end point.

private slots:
    void requestFinished(QNetworkReply *reply);
};


/*!
 * \brief Constructs an instance of QUpYun with given \a parent.
 *
 * The bucket name is \a bucketName, user name is \a userName and
 * password is \a password.
 *
 * \note Password need not compute MD5 value.
 */
QUpYun::QUpYun(const QString &bucketName,
               const QString &userName,
               const QString &password,
               QObject *parent) :
    QObject(parent),
    d(new Private(this))
{
    d->bucketName = bucketName;
    d->userName = userName;
    d->password = QString(d->md5(password.toUtf8()));
}

/*!
 * \brief Destroys the instance.
 */
QUpYun::~QUpYun()
{
    delete d;
}

/*!
 * \brief Returns SDK version.
 */
inline QString QUpYun::version() const
{
    return QLatin1String(SDK_VERSION);
}

/*!
 * \brief Sets API domain to \a ed.
 */
inline void QUpYun::setAPIDomain(EndPoint ed)
{
    d->apiDomain = ed;
}

/*!
 * \brief Returns current API domein.
 */
inline QUpYun::EndPoint QUpYun::apiDomain() const
{
    return d->apiDomain;
}

/*!
 * \brief Gets the usage of this bucket.
 *
 * \sa QUpYun::requestBucketUsageFinished(qulonglong)
 */
void QUpYun::bucketUsage()
{
    QNetworkReply *reply = d->sendRequest(QNetworkAccessManager::GetOperation,
                                          QString("%1?usage").arg(d->formatPath("/")));
    d->requests.insert(reply, BucketUsage);
}

/*!
 * \brief Makes a directory at \a path.
 *
 * Sets \a autoMkdir to true if auto mkdir needed.
 *
 * \sa QUpYun::requestMkdirFinished(bool)
 */
void QUpYun::mkdir(const QString &path, bool autoMkdir)
{
    RequestParams params;
    params.insert(MKDIR, QLatin1String("true"));
    QNetworkReply *reply = d->sendRequest(QNetworkAccessManager::PutOperation,
                                          d->formatPath(path),
                                          QByteArray(),
                                          autoMkdir,
                                          params);
    d->requests.insert(reply, Mkdir);
}

/*!
 * \brief Removes directory at \a path.
 *
 * \sa QUpYun::requestRmdirFinished(bool)
 */
void QUpYun::rmdir(const QString &path)
{
    QNetworkReply *reply = d->sendRequest(QNetworkAccessManager::DeleteOperation,
                                          d->formatPath(path));
    d->requests.insert(reply, Rmdir);
}

/*!
 * \brief Lists directory at \a path.
 */
void QUpYun::ls(const QString &path)
{
    QNetworkReply *reply = d->sendRequest(QNetworkAccessManager::GetOperation,
                                          path.endsWith(SEPARATOR)
                                            ? d->formatPath(path)
                                            : d->formatPath(path) + SEPARATOR);
    d->requests.insert(reply, Ls);
}

/*!
 * \brief Uploads a file at \a localPath to \a path.
 *
 * \a Path should start with '/', append by bucket name and file path.
 * eg: /demobucket/upload.png. If the path is a directory, it MUST end with '/'.
 *
 * Sets \a autoMkdir to true if auto mkdir needed (10 at most).
 *
 * Sets \a appendFileMD5 to true if append file MD5 value needed. If this value
 * is different from the server computed, 406 Not Accepted will be recieved.
 *
 * Sets \a fileSecret as file secret key. ONLY picture spaces support secret.
 * If this is set, the file could not access directly by URL,
 * use THUMB_SEPERATOR + SECRET instead. eg, if THUMB_SEPERATOR is "!",
 * secret is "bac" and file upload path is "/folder/test.jpg", the image accessed
 * URL will be http://BUCKET/folder/test.jpg!bac.
 *
 * Extra parameters should be stored in \a params.
 *
 * \sa QUpYun::uploadFile(const QString &, QFile *, bool, bool, const QString &, const RequestParams &)
 * \sa QUpYun::requestUploadFinished(bool, const PicInfo &)
 */
void QUpYun::uploadFile(const QString &path,
                        const QString &localPath,
                        bool autoMkdir,
                        bool appendFileMD5,
                        const QString &fileSecret,
                        const RequestParams &params)
{
    QFile file(localPath);
    uploadFile(path, &file, autoMkdir, appendFileMD5, fileSecret, params);
}

/*!
 * \brief Uploads a \a file to \a path.
 *
 * \a Path should start with '/', append by bucket name and file path.
 * eg: /demobucket/upload.png. If the path is a directory, it MUST end with '/'.
 *
 * Sets \a autoMkdir to true if auto mkdir needed (10 at most).
 *
 * Sets \a appendFileMD5 to true if append file MD5 value needed. If this value
 * is different from the server computed, 406 Not Accepted will be recieved.
 *
 * Sets \a fileSecret as file secret key. ONLY picture spaces support secret.
 * If this is set, the file could not access directly by URL,
 * use THUMB_SEPERATOR + SECRET instead. eg, if THUMB_SEPERATOR is "!",
 * secret is "bac" and file upload path is "/folder/test.jpg", the image accessed
 * URL will be http://BUCKET/folder/test.jpg!bac.
 *
 * Extra parameters should be stored in \a params.
 *
 * \sa QUpYun::uploadFile(const QString &, const QString &, bool, bool, const QString &, const RequestParams &)
 * \sa QUpYun::requestUploadFinished(bool, const PicInfo &)
 */
void QUpYun::uploadFile(const QString &path,
                        QFile *file,
                        bool autoMkdir,
                        bool appendFileMD5,
                        const QString &fileSecret,
                        const RequestParams &params)
{
    if (!file->isOpen()) {
        file->open(QFile::ReadOnly);
    }
    RequestParams newParams(params);
    QByteArray data = file->readAll();
    if (appendFileMD5) {
        static QByteArray CONTENT_MD5("Content-MD5");
        newParams.insert(CONTENT_MD5, d->md5(data));
    }
    if (!fileSecret.isEmpty()) {
        static QByteArray CONTENT_SECRET("Content-Secret");
        newParams.insert(CONTENT_SECRET, fileSecret.toUtf8());
    }
    QNetworkReply *reply = d->sendRequest(QNetworkAccessManager::PutOperation,
                                          d->formatPath(path),
                                          data,
                                          autoMkdir,
                                          newParams);
    d->requests.insert(reply, Upload);
}

/*!
 * \brief Downloads file from \a path.
 *
 * \sa QUpYun::requestDownloadFinished(const QByteArray &)
 */
void QUpYun::downloadFile(const QString &path)
{
    QNetworkReply *reply = d->sendRequest(QNetworkAccessManager::GetOperation,
                                          d->formatPath(path));
    d->requests.insert(reply, Read);
}

/*!
 * \brief Removes file at \a filePath.
 *
 * \sa QUpYun::requestRemoveFileFinished(bool)
 */
void QUpYun::removeFile(const QString &filePath)
{
    QNetworkReply *reply = d->sendRequest(QNetworkAccessManager::DeleteOperation,
                                          d->formatPath(filePath));
    d->requests.insert(reply, RemoveFile);
}

/*!
 * \brief Gets information of the file at \a filePath.
 *
 * \sa QUpYun::requestFileInfoFinished(const FileInfo &)
 */
void QUpYun::fileInfo(const QString &filePath)
{
    QNetworkReply *reply = d->sendRequest(QNetworkAccessManager::HeadOperation,
                                          d->formatPath(filePath));
    d->requests.insert(reply, FileProp);
}

#include "qupyun.moc"

QString QUpYun::Private::upyunAPIDomain() const
{
    switch (apiDomain) {
    case QUpYun::ED_AUTO:
        return QLatin1String("v0.api.upyun.com");
    case QUpYun::ED_TELECOM:
        return QLatin1String("v1.api.upyun.com");
    case QUpYun::ED_CNC:
        return QLatin1String("v2.api.upyun.com");
    case QUpYun::ED_CTT:
        return QLatin1String("v3.api.upyun.com");
    default:
        return QLatin1String("");
    }
}

QNetworkReply * QUpYun::Private::sendRequest(QNetworkAccessManager::Operation method,
                                            const QString &uri,
                                            const QByteArray &data,
                                            bool autoMkdir,
                                            const RequestParams &params)
{
    Q_ASSERT(method == QNetworkAccessManager::GetOperation
             || method == QNetworkAccessManager::PutOperation
             || method == QNetworkAccessManager::HeadOperation
             || method == QNetworkAccessManager::DeleteOperation);

    static QByteArray DATE("Date");
    static QByteArray AUTHORIZATION("Authorization");
    static QByteArray MKDIR("mkdir");

    QByteArray date = getGMTDate();

    QNetworkRequest request;
    request.setUrl(QUrl(QString("http://%1%2").arg(upyunAPIDomain(), uri)));
    request.setRawHeader(DATE, date);

    // mkdir
    if (autoMkdir) {
        request.setRawHeader(MKDIR, QByteArray("true"));
    }

    qlonglong contentLength = data.length();
    // set content length
    request.setHeader(QNetworkRequest::ContentLengthHeader, contentLength);
    // set signature
    request.setRawHeader(AUTHORIZATION, signature(method, QString(date), uri, contentLength));
    // set extra params
    bool isFolder = false;
    if (!params.isEmpty()) {
        isFolder = params.value(MKDIR).toBool();
        QHash<QString, QVariant>::const_iterator i = params.constBegin();
        while (i != params.constEnd()) {
            request.setRawHeader(i.key().toUtf8(), i.value().toByteArray());
            ++i;
        }
    }

#ifdef QT_DEBUG
    qDebug() << request.url().toString();
    QList<QByteArray> rawHeaders = request.rawHeaderList();
    foreach (QByteArray header, rawHeaders) {
        qDebug() << header << ": " << request.rawHeader(header);
    }
    qDebug() << "---------- Request Data Finished ----------";
#endif

    QNetworkReply *reply = 0;
    switch (method) {
    case QNetworkAccessManager::GetOperation:
        reply = manager->get(request);
        break;
    case QNetworkAccessManager::PutOperation:
        reply = manager->put(request, data);
        break;
    case QNetworkAccessManager::HeadOperation:
        reply = manager->head(request);
        break;
    case QNetworkAccessManager::DeleteOperation:
        reply = manager->deleteResource(request);
        break;
    default:
        // do nothing
        break;
    }
    return reply;
}

inline QString QUpYun::Private::formatPath(const QString &path) const
{
    QString formatted;
    if (!path.isEmpty()) {
        formatted = path.trimmed();
        if (!formatted.startsWith(SEPARATOR)) {
            return SEPARATOR + bucketName + SEPARATOR + formatted;
        }
    }
    return SEPARATOR + bucketName + formatted;
}

inline QByteArray QUpYun::Private::md5(const QByteArray &data) const
{
    return QCryptographicHash::hash(data,
                                    QCryptographicHash::Md5).toHex();
}

inline QByteArray QUpYun::Private::getGMTDate() const
{
    QString dateTimeString = QLocale::c().toString(QDateTime::currentDateTimeUtc(),
                                                   "ddd, dd MMM yyyy hh:mm:ss");
    return QString("%1 GMT").arg(dateTimeString).toUtf8();
}

inline QByteArray QUpYun::Private::signature(QNetworkAccessManager::Operation method,
                                            const QString &date,
                                            const QString &uri,
                                            qlonglong length) const
{
    Q_ASSERT(method == QNetworkAccessManager::GetOperation
             || method == QNetworkAccessManager::PutOperation
             || method == QNetworkAccessManager::HeadOperation
             || method == QNetworkAccessManager::DeleteOperation);
    QString methodName;
    switch (method) {
    case QNetworkAccessManager::GetOperation:
        methodName = QString("GET");
        break;
    case QNetworkAccessManager::PutOperation:
        methodName = QString("PUT");
        break;
    case QNetworkAccessManager::HeadOperation:
        methodName = QString("HEAD");
        break;
    case QNetworkAccessManager::DeleteOperation:
        methodName = QString("DELETE");
        break;
    default:
        // do nothing
        break;
    }
    QString sign = QString("%1&%2&%3&%4&%5").arg(methodName,
                                                 uri,
                                                 date,
                                                 QString::number(length),
                                                 password);
    return QString("UpYun %1:%2").arg(userName, QString(md5(sign.toUtf8()))).toUtf8();
}

void QUpYun::Private::requestFinished(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
#ifdef QT_DEBUG
    qDebug() << data
             << reply->rawHeaderPairs();
#endif
    if (reply->error() == QNetworkReply::NoError) {
        API currentAPI = requests.value(reply);
        switch (currentAPI) {
        case BucketUsage:
            {
            emit q->requestBucketUsageFinished(data.toULongLong());
            break;
            }
        case Mkdir:
            {
            emit q->requestMkdirFinished(data.isEmpty());
            break;
            }
        case Rmdir:
            {
            emit q->requestRmdirFinished(data.isEmpty());
            break;
            }
        case Ls:
        {
            QList<ItemInfo> infos;
            QString result(data);
            QStringList entries = result.split('\n');
            foreach (QString entry, entries) {
                QStringList items = entry.split('\t');
                ItemInfo info;
                info.name = items.at(0);
                info.isFolder = items.at(1).compare(QLatin1String("F"), Qt::CaseInsensitive) == 0;
                info.size = items.at(2).toULongLong();
                info.date = QDateTime::fromTime_t(items.at(3).toUInt());
                infos << info;
            }
            emit q->requestLsFinished(infos);
            break;
        }
        case Upload:
            {
            static QByteArray PIC_TYPE("x-upyun-file-type");
            static QByteArray PIC_WIDTH("x-upyun-width");
            static QByteArray PIC_HEIGHT("x-upyun-height");
            static QByteArray PIC_FRAMES("x-upyun-frames");

            PicInfo info;
            info.type = QString(reply->rawHeader(PIC_TYPE));
            info.width = reply->rawHeader(PIC_WIDTH).toULongLong();
            info.height = reply->rawHeader(PIC_HEIGHT).toULongLong();
            info.frames = reply->rawHeader(PIC_FRAMES).toULongLong();

            emit q->requestUploadFinished(data.isEmpty(), info);
            break;
            }
        case Read:
            {
            emit q->requestDownloadFinished(data);
            break;
            }
        case RemoveFile:
            {
            emit q->requestRemoveFileFinished(data.isEmpty());
            break;
            }
        case FileProp:
            {
            static QByteArray FILE_TYPE("x-upyun-file-type");
            static QByteArray FILE_SIZE("x-upyun-file-size");
            static QByteArray FILE_DATE("x-upyun-file-date");

            FileInfo info;
            info.type = QString(reply->rawHeader(FILE_TYPE));
            info.size = reply->rawHeader(FILE_SIZE).toULongLong();
            info.createDate = QDateTime::fromTime_t(reply->rawHeader(FILE_DATE).toUInt());
            emit q->requestFileInfoFinished(info);
            break;
            }
        default:
            // do nothing
            break;
        }
    } else {
        // something wrong
        emit q->requestError(reply->error(), reply->errorString());
    }
   reply->deleteLater();
}

QDebug operator<<(QDebug dbg, const FileInfo &fileInfo)
{
    dbg.nospace()
            << "FileInfo ("
            << "type=" << fileInfo.type << ", "
            << "size=" << fileInfo.size << ", "
            << "createDate=" << fileInfo.createDate << ")";
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const PicInfo &picInfo)
{
    dbg.nospace()
            << "PicInfo ("
            << "type=" << picInfo.type << ", "
            << "width=" << picInfo.width << ", "
            << "height=" << picInfo.height << ", "
            << "frames=" << picInfo.frames << ")";
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const ItemInfo &itemInfo)
{
    dbg.nospace()
            << "ItemInfo ("
            << "name=" << itemInfo.name << ", "
            << "isFolder=" << itemInfo.isFolder << ", "
            << "size=" << itemInfo.size << ", "
            << "date=" << itemInfo.date << ")";
    return dbg.space();
}
