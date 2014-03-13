#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QStringList>

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
    inline QByteArray extraParamHeader(QUpYun::ExtraParam param) const;

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

QByteArray QUpYun::Private::extraParamHeader(QUpYun::ExtraParam param) const
{
    switch (param) {
        case X_GMKERL_TYPE:
            return QByteArray("x-gmkerl-type");
        case X_GMKERL_VALUE:
            return QByteArray("x-gmkerl-value");
        case X_GMKERL_QUALITY:
            return QByteArray("x-gmkerl-quality");
        case X_GMKERL_UNSHARP:
            return QByteArray("x-gmkerl-unsharp");
        case X_GMKERL_THUMBNAIL:
            return QByteArray("x-gmkerl-thumbnail");
        case X_GMKERL_ROTATE:
            return QByteArray("x-gmkerl-rotate");
        case X_GMKERL_CROP:
            return QByteArray("x-gmkerl-crop");
        case X_GMKERL_EXIF_SWITCH:
            return QByteArray("x-gmkerl-exif-switch");
        case FIX_MAX:
            return QByteArray("fix_max");
        case FIX_MIN:
            return QByteArray("fix_min");
        case FIX_WIDTH_OR_HEIGHT:
            return QByteArray("fix_width_or_height");
        case FIX_WIDTH:
            return QByteArray("fix_width");
        case FIX_HEIGHT:
            return QByteArray("fix_height");
        case SQUARE:
            return QByteArray("square");
        case FIX_BOTH:
            return QByteArray("fix_both");
        case FIX_SCALE:
            return QByteArray("fix_scale");
        case ROTATE_AUTO:
            return QByteArray("auto");
        case ROTATE_90:
            return QByteArray("90");
        case ROTATE_180:
            return QByteArray("180");
        case ROTATE_270:
            return QByteArray("270");
    default:
        break;
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

/*!
 * \struct FileInfo
 * \brief File information.
 */

/*!
 * \var QString FileInfo::type
 * \brief Returns "file" if is a file, "folder" if folder.
 */

/*!
 * \var qulonglong FileInfo::size
 * \brief Returns file size.
 */

/*!
 * \var QDateTime FileInfo::createDate
 * \brief Returns file create date.
 */


/*!
 * \struct PicInfo
 * \brief Picture information.
 */

/*!
 * \var QString PicInfo::type
 * \brief Returns picture type.
 */

/*!
 * \var qulonglong PicInfo::width
 * \brief Returns picture width.
 */

/*!
 * \var qulonglong PicInfo::height
 * \brief Returns picture height.
 */

/*!
 * \var qulonglong PicInfo::frames
 * \brief Returns picture frames amount.
 */


/*!
 * \struct ItemInfo
 * \brief Item information.
 */

/*!
 * \var QString ItemInfo::name
 * \brief Returns item name.
 */

/*!
 * \var bool ItemInfo::isFolder
 * \brief Returns true if this item is a folder.
 */

/*!
 * \var qulonglong ItemInfo::size
 * \brief Returns item size.
 */

/*!
 * \var QDateTime PicInfo::date
 * \brief Returns item date.
 */

/*!
 * \enum QUpYun::EndPoint
 * \brief End point of UpYun.
 */

/*!
 * \var QUpYun::EndPoint QUpYun::ED_AUTO
 * \brief Selects end point by network automatically.
 *
 * This will set end point to v0.api.upyun.com.
 */

/*!
 * \var QUpYun::EndPoint QUpYun::ED_TELECOM
 * \brief Telecom end point.
 *
 * This will set end point to v1.api.upyun.com.
 */

/*!
 * \var QUpYun::EndPoint QUpYun::ED_CNC
 * \brief CNC end point.
 *
 * This will set end point to v2.api.upyun.com.
 */

/*!
 * \var QUpYun::EndPoint QUpYun::ED_CTT
 * \brief CTT end point.
 *
 * This will set end point to v3.api.upyun.com.
 */


/*!
 * \enum QUpYun::ExtraParam
 * \brief Valid extra parameters when uploads a file.
 *
 * Uses these extra parameters with a file which is not a picture will cause
 * an upload failure.
 *
 * \note Original picture will not be saved if append any extra parameters.
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::X_GMKERL_TYPE
 * \brief Thumbs type.
 *
 * Uses this parameter if original pictures are unnecessary, eg. account images.
 *
 * Valid values are:
 *
 * \li \c FIX_MAX : Fix maximum edge, shorter edges suit by themselves.
 * \li \c FIX_MIN : Fix minimum edge, longer edges suit by themselves.
 * \li \c FIX_WIDTH_OR_HEIGHT : Fix both width and height. Do not scale if not long enough.
 * \li \c FIX_WIDTH : Fix width, height suits by itself.
 * \li \c FIX_HEIGHT : Fix height, width suits by itself.
 * \li \c FIX_BOTH : Fix both width and height. Scales if not long enough.
 * \li \c FIX_SCALE : Scale (1-99).
 * \li \c SQUARE : Scale to square.
 *
 * \note This parameter MUST be with \c X_GMKERL_VALUE together,
 * otherwise it is unavaliable.
 *
 * \sa QUpYun::X_GMKERL_VALUE
 * \sa http://wiki.upyun.com/index.php?title=%E7%BC%A9%E7%95%A5%E5%9B%BE%E6%96%B9%E5%BC%8F%E5%B7%AE%E5%88%AB%E4%B8%BE%E4%BE%8B
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::X_GMKERL_VALUE
 * \brief Thumbs value.
 *
 * If \c X_GMKERL_TYPE is \c FIX_WIDTH_OR_HEIGHT or \c FIX_BOTH, the value
 * should be widthxheight (eg. 200x150); otherwise be a number (eg. 150).
 *
 * \note This parameter MUST be with \c X_GMKERL_TYPE together,
 * otherwise it is unavaliable.
 *
 * \sa QUpYun::X_GMKERL_TYPE
 * \sa http://wiki.upyun.com/index.php?title=%E7%BC%A9%E7%95%A5%E5%9B%BE%E6%96%B9%E5%BC%8F%E5%B7%AE%E5%88%AB%E4%B8%BE%E4%BE%8B
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::X_GMKERL_QUALITY
 * \brief Thumbs quality (1-100). 95 by default.
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::X_GMKERL_UNSHARP
 * \brief Image sharp. \c true by default.
 *
 * Valid values are \c true and \c false.
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::X_GMKERL_THUMBNAIL
 * \brief Generates thumbnail.
 * \note Defines thumb version before use this parameter.
 * \sa http://wiki.upyun.com/index.php?title=%E5%A6%82%E4%BD%95%E5%88%9B%E5%BB%BA%E8%87%AA%E5%AE%9A%E4%B9%89%E7%BC%A9%E7%95%A5%E5%9B%BE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::X_GMKERL_ROTATE
 * \brief Rotate pictures.
 *
 * Valid values are:
 * \li \c ROTATE_AUTO : Automatically rotates by EXIF. If there is no EXIF,
 * this value is invalid.
 * \li \c ROTATE_90 : Rotates by 90.
 * \li \c ROTATE_180 : Rotates by 180.
 * \li \c ROTATE_270 : Rotates by 270.
 *
 * \sa QUpYun::ROTATE_AUTO
 * \sa QUpYun::ROTATE_90
 * \sa QUpYun::ROTATE_180
 * \sa QUpYun::ROTATE_270
 * \sa http://wiki.upyun.com/index.php?title=%E5%9B%BE%E7%89%87%E6%97%8B%E8%BD%AC
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::X_GMKERL_CROP
 * \brief Crop picture.
 *
 * Valid value format is x,y,width,height (eg. 0,0,100,200).
 * And x >= 0 && y >=0 && width > 0 && height
 *
 * \sa http://wiki.upyun.com/index.php?title=%E5%9B%BE%E7%89%87%E8%A3%81%E5%89%AA
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::X_GMKERL_EXIF_SWITCH
 * \brief Stores EXIF.
 *
 * Valid values are \c true and \c false.
 *
 * UpYun will remove EXIF if the picture crops, scales or thumbnails, use this
 * parameter if want to store EXIF.
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::FIX_MAX
 * \brief Fix maximum edge, shorter edges suit by themselves.
 * \sa QUpYun::X_GMKERL_TYPE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::FIX_MIN
 * \brief Fix minimum edge, longer edges suit by themselves.
 * \sa QUpYun::X_GMKERL_TYPE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::FIX_WIDTH_OR_HEIGHT
 * \brief Fix both width and height. Do not scale if not long enough.
 * \sa QUpYun::X_GMKERL_TYPE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::FIX_WIDTH
 * \brief Fix width, height suits by itself.
 * \sa QUpYun::X_GMKERL_TYPE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::FIX_HEIGHT
 * \brief Fix height, width suits by itself.
 * \sa QUpYun::X_GMKERL_TYPE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::SQUARE
 * \brief Scale to square.
 * \sa QUpYun::X_GMKERL_TYPE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::FIX_BOTH
 * \brief Fix both width and height. Scales if not long enough.
 * \sa QUpYun::X_GMKERL_TYPE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::FIX_SCALE
 * \brief Scale (1-99).
 * \sa QUpYun::X_GMKERL_TYPE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::ROTATE_AUTO
 * \brief Automatically rotate by EXIF. If there is no EXIF,
 * this value is invalid.
 * \sa QUpYun::X_GMKERL_ROTATE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::ROTATE_90
 * \brief Rotates by 90.
 * \sa QUpYun::X_GMKERL_ROTATE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::ROTATE_180
 * \brief Rotates by 180.
 * \sa QUpYun::X_GMKERL_ROTATE
 */

/*!
 * \var QUpYun::ExtraParam QUpYun::ROTATE_270
 * \brief Rotates by 270.
 * \sa QUpYun::X_GMKERL_ROTATE
 */
