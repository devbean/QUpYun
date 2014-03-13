#ifndef QUPYUN_H
#define QUPYUN_H

#include <QDateTime>
#include <QNetworkReply>
#include <QObject>

#include "qupyun_global.h"

QT_BEGIN_NAMESPACE
class QFile;
QT_END_NAMESPACE

struct FileInfo
{
    QString    type;
    qulonglong size;
    QDateTime  createDate;
};
QDebug operator<<(QDebug dbg, const FileInfo &fileInfo);
Q_DECLARE_METATYPE(FileInfo)

struct PicInfo
{
    QString    type;
    qulonglong width;
    qulonglong height;
    qulonglong frames;
};
QDebug operator<<(QDebug dbg, const PicInfo &picInfo);
Q_DECLARE_METATYPE(PicInfo)

struct ItemInfo
{
    QString name;
    bool isFolder;
    qulonglong size;
    QDateTime date;
};
QDebug operator<<(QDebug dbg, const ItemInfo &itemInfo);
Q_DECLARE_METATYPE(ItemInfo)


class QUPYUNSHARED_EXPORT QUpYun : public QObject
{
    Q_OBJECT
public:
    typedef QHash<QString, QVariant> RequestParams;

    enum EndPoint
    {
        ED_AUTO = 0,
        ED_TELECOM,
        ED_CNC,
        ED_CTT
    };

    enum ExtraParam
    {
        X_GMKERL_TYPE,
        X_GMKERL_VALUE,
        X_GMKERL_QUALITY,
        X_GMKERL_UNSHARP,
        X_GMKERL_THUMBNAIL,
        X_GMKERL_ROTATE,
        X_GMKERL_CROP,
        X_GMKERL_EXIF_SWITCH,

        FIX_MAX,
        FIX_MIN,
        FIX_WIDTH_OR_HEIGHT,
        FIX_WIDTH,
        FIX_HEIGHT,
        SQUARE,
        FIX_BOTH,
        FIX_SCALE,
        ROTATE_AUTO,
        ROTATE_90,
        ROTATE_180,
        ROTATE_270
    };

    QUpYun(const QString &bucketName,
           const QString &userName,
           const QString &password,
           QObject *parent = 0);
    ~QUpYun();

    inline QString version() const;

    inline void setAPIDomain(EndPoint ed);
    inline EndPoint apiDomain() const;

    void bucketUsage();

    void mkdir(const QString &path, bool autoMkdir = false);
    void rmdir(const QString &path);
    void ls(const QString &path);

    void uploadFile(const QString &path,
                    const QString &localPath,
                    bool autoMkdir = false,
                    bool appendFileMD5 = false,
                    const QString &fileSecret = QString(),
                    const RequestParams &params = RequestParams());
    void uploadFile(const QString &path,
                    QFile *file,
                    bool autoMkdir = false,
                    bool appendFileMD5 = false,
                    const QString &fileSecret = QString(),
                    const RequestParams &params = RequestParams());
    void downloadFile(const QString &path);
    void removeFile(const QString &filePath);

    void fileInfo(const QString &filePath);

signals:
    void requestError(QNetworkReply::NetworkError errorCode,
                      const QString &errorMessage);

    void requestBucketUsageFinished(qulonglong usage);
    void requestMkdirFinished(bool success);
    void requestRmdirFinished(bool success);
    void requestLsFinished(const QList<ItemInfo> &itemInfos);
    void requestUploadFinished(bool success, const PicInfo &picInfo);
    void requestDownloadFinished(const QByteArray &data);
    void requestRemoveFileFinished(bool success);
    void requestFileInfoFinished(const FileInfo &fileInfo);

private:
    class Private;
    QUpYun::Private *d;
    friend class QUpYun::Private;
};

#endif // QUPYUN_H
