# QUpYun - 又拍云Qt SDK
======

`QUpYun`是又拍云存储的Qt SDK，基于[又拍云存储HTTP REST API接口](http://wiki.upyun.com/index.php?title=HTTP_REST_API接口)开发，适用于Qt 5.x版本。

`QUpYun`使用`QNetworkAccessManager`进行网络访问。通常，一个应用程序仅需要一个`QUpYun`实例即可满足需要，不需要创建多个`QUpYun`对象。`QUpYun`提供的访问接口全部为异步访问模式，程序开发人员需要连接相应的信号获得又拍云服务器返回的数据。与此同时，`QUpYun`还提供了`requestError(QNetworkReply::NetworkError, const QString &)`信号，用于处理可能发生的错误。

## 目录
* [云存储基础接口](#云存储基础接口)
  * [准备操作](#准备操作)
  * [上传文件](#上传文件)
  * [下载文件](#下载文件)
  * [获取文件信息](#获取文件信息)
  * [删除文件](#删除文件)
  * [创建目录](#创建目录)
  * [删除目录](#删除目录)
  * [获取目录文件列表](#获取目录文件列表)
  * [获取空间使用量情况](#获取空间使用量情况)
  
<a name="云存储基础接口"></a>
## 云存储基础接口

<a name="准备操作"></a>
### 准备操作
`QUpYun`的全部代码位于`source`目录中。在需要使用`QUpYun`的工程文件中引入`qupyun.pri`即可正常使用：
```
include("qupyun.pri")
```

源代码文件中，我们可以通过下面语句引入`QUpYun`：
```cpp
#include <QUpYun>
```
##### 创建空间
用户需要自行在[又拍云网站](https://www.upyun.com/login.php)创建个性化空间。具体教程请参见[创建空间](http://wiki.upyun.com/index.php?title=创建空间)。

##### 初始化QUpYun
```
QUpYun *upyun = new QUpYun(BucketName, Operator, Password, parent);
```
若不了解“授权操作员”，请参见[授权操作员](http://wiki.upyun.com/index.php?title=创建操作员并授权)

##### 选择最优的接入点
根据国内的网络情况，又拍云存储API目前提供了电信、联通网通、移动铁通三个接入点。我们可以通过`QUpYun`的接口进行设置：
```
    upyun->setAPIDomain(QUpYun::ED_AUTO);
```
若没有明确进行设置，`QUpYun`默认将根据网络条件自动选择接入点。使用`apiDomain()`函数可以获取当前设置的接入点。

接入点有四个值可选：

* **QUpYun::ED_AUTO** ：根据网络条件自动选择接入点
* **QUpYun::ED_TELECOM** ：电信接入点
* **QUpYun::ED_CNC** ：联通网通接入点
* **QUpYun::ED_CTT** ：移动铁通接入点

_**注：**建议根据服务器网络状况，手动设置合理的接入点已获取最佳的访问速度。_

<a name="上传文件"></a>
### 上传文件

##### 基本用法

`QUpYun`提供了两个用于上传的函数：
```
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
```
我们可以根据使用=文件路径还是文件对象区别使用这两个函数，例如：
```
QUpYun *upyun = new QUpYun(BucketName, Operator, Password, parent);
connect(upyun, &QUpYun::requestUploadFinished, [=] (bool success, const PicInfo &picInfo) {
	...
});
						   
// 使用文件所在路径
upyun->uploadFile(savePath, localFilePath);

// 上传文件对象
QFile file(localFilePath);
upyun->uploadFile(savePath, &file);
```

##### 参数说明
* `savePath`：上传到的又拍云存储的具体地址
  * 比如`/dir/sample.jpg`表示以`sample.jpg`为文件名保存到`/dir`目录下；
  * 若保存路径为`/sample.jpg`，则表示保存到根目录下；
  * **注意`savePath`必须以`/`开始**，下同。
* 第二个参数可以是`QString`或`QFile *`类型，即需要上传的文件路径或文件本身。
* `autoMkdir`：可选的`bool`类型参数，表示当不存在父级目录时是否自动创建父级目录（只支持自动创建10级以内的父级目录）。
* `appendFileMD5`：可选的`bool`类型参数，表示是否需要附加文件的MD5校验值。如果设置了该参数，上传时会附加被上传文件的MD5校验值。若又拍云服务器计算而得的文件MD5值与此不同，则服务器返回`406 Not Acceptable`错误。对于需要确保上传文件的完整性要求的业务，可以设置该参数。
* `fileSecret`：可选的`QString`类型参数，表示是否添加文件访问密钥。该访问密钥仅支持图片空间。图片类空间若设置过[缩略图版本号](http://wiki.upyun.com/index.php?title=如何创建自定义缩略图)，即可使用原图保护功能（**文件类空间无效**）。如果设置了该参数，上传时会附加文件访问密钥。待文件保存成功后，将无法根据`http://空间名.b0.upaiyun.com/savePath`直接访问上传的文件，而是需要在URL后面加上`缩略图间隔标志符+密钥`进行访问。例如，[缩略图间隔标志符](http://wiki.upyun.com/index.php?title=如何使用自定义缩略图)为`!`，密钥为`abc`，上传的文件路径为`/dir/sample.jpg`，那么该图片的对外访问地址为: `http://空间名.b0.upaiyun.com/dir/sample.jpg!abc`。**注意，原图保护密钥若与[缩略图版本号](http://wiki.upyun.com/index.php?title=如何创建自定义缩略图)名称相同，则在对外访问时将被视为是缩略图功能，而原图将无法访问，请慎重使用。**
* `params`：上传图片时，允许直接对图片进行旋转、裁剪、缩略图等操作，具体请参见[图片处理接口](#图片处理接口)。

##### 其他说明
* 文件上传成功后，可直接通过`http://空间名.b0.upaiyun.com/savePath`（或设置的自定义域名）访问文件。
* 图片类空间上传文件后，会返回文件的基本信息，可通过信号的`PicInfo`参数获取。`PicInfo`会返回以下信息：
  * **QString type**：图片类型
  * **qulonglong width**：图片宽度
  * **qulonglong height**：图片高度
  * **qulonglong frames**：图片帧数

##### 注意事项
* 如果空间内`savePath`已经存在文件，将进行覆盖操作，并且是**不可逆**的。所以如果需要避免文件被覆盖的情况，可以先通过[获取文件信息](#获取文件信息)操作来判断是否已经存在旧文件。
* 图片类空间只允许上传图片类文件，其他文件上传时将返回“不是图片”的错误。
* 使用`requestError(QNetworkReply::NetworkError, const QString &)`信号处理错误。

<a name="下载文件"></a>
### 下载文件
```
QUpYun *upyun = new QUpYun(BucketName, Operator, Password, parent);
connect(upyun, &QUpYun::requestDownloadFinished, [=] (const QByteArray &data) {
	...
});

upyun->downloadFile(savePath);
```

##### 参数说明
* `savePath`：又拍云存储中文件的具体保存地址。比如`/dir/sample.jpg`。

##### 注意事项
* 下载文件时必须确保空间下存在该文件，否则将返回`文件不存在`错误。
* 使用`requestError(QNetworkReply::NetworkError, const QString &)`信号处理错误。

<a name="获取文件信息"></a>
### 获取文件信息
```
QUpYun *upyun = new QUpYun(BucketName, Operator, Password, parent);
connect(upyun, &QUpYun::requestFileInfoFinished, [=] (const FileInfo &fileInfo) {
	...
});

upyun->downloadFile(savePath);
```
`FileInfo`可以获得以下文件信息：
* **QString type**：文件类型
* **qulonglong size**：文件大小
* **QDateTime createDate**：创建日期

##### 参数说明
* `savePath`：又拍云存储中文件的具体保存地址。比如`/dir/sample.jpg`。

##### 其他说明
* 若文件不存在，则返回错误。
* 使用`requestError(QNetworkReply::NetworkError, const QString &)`信号处理错误。

<a name="删除文件"></a>
### 删除文件
```
QUpYun *upyun = new QUpYun(BucketName, Operator, Password, parent);
connect(upyun, &QUpYun::requestRemoveFileFinished, [=] (bool success) {
	...
});

upyun->removeFile(savePath);
```
    
##### 参数说明
* `savePath`：又拍云存储中文件的具体保存地址。比如`/dir/sample.jpg`。

##### 其他说明
* 删除文件时必须确保空间下存在该文件，否则将返回`文件不存在`的错误。
* 使用`requestError(QNetworkReply::NetworkError, const QString &)`信号处理错误。

<a name="创建目录"></a>
### 创建目录
```
QUpYun *upyun = new QUpYun(BucketName, Operator, Password, parent);
connect(upyun, &QUpYun::requestMkdirFinished, [=] (bool success) {
	...
});

upyun->mkdir(dir, autoMkdir);
```
    
##### 参数说明
* `dir`：待创建的目录结构。比如`/dir1/dir2/dir3/`。
* `autoMkdir`：可选的`boolean`类型参数，表示当不存在父级目录时是否自动创建父级目录（只支持自动创建10级以内的父级目录）。

##### 其他说明
* 待创建的目录路径必须以斜杠 `/` 结尾。
* 创建目录操作可在[上传文件](#上传文件)时一并完成，功能是相同的。
* 若空间相同目录下已经存在同名的文件，则将返回`不允许创建目录`的错误。
* 使用`requestError(QNetworkReply::NetworkError, const QString &)`信号处理错误。

<a name="删除目录"></a>
### 删除目录
```
QUpYun *upyun = new QUpYun(BucketName, Operator, Password, parent);
connect(upyun, &QUpYun::requestRmdirFinished, [=] (bool success) {
	...
});

upyun->rmdir(dir);
```

##### 参数说明
* `dir`：待删除的目录结构。比如`/dir1/dir2/dir3/`。

##### 其他说明
* 该操作只能删除单级目录，不能一次性同时删除多级目录，比如当存在`/dir1/dir2/dir3/`目录时，不能试图只传递`/dir1/`来删除所有目录。
* 若待删除的目录`dir`下还存在任何文件或子目录，将返回`不允许删除`的错误。比如当存在`/dir1/dir2/dir3/`目录时，将无法删除`/dir1/dir2/`目录。
* 使用`requestError(QNetworkReply::NetworkError, const QString &)`信号处理错误。


<a name="获取目录文件列表"></a>
### 获取目录文件列表
```
QUpYun *upyun = new QUpYun(BucketName, Operator, Password, parent);
connect(upyun, &QUpYun::requestLsFinished, [=] (const QList<ItemInfo> &itemInfos) {
	...
});

upyun->ls(dir);
```
`ItemInfo`可以获得以下信息：
* **QString name**：文件名字
* **bool isFolder**：是否目录
* **qulonglong size**：文件大小
* **QDateTime date**：文件时间

##### 参数说明
* `dir`：待查询的目录结构。比如`/dir1/`。

##### 其他说明
* 若`dir`目录不存在任何内容时，将返回空列表。
* 若`dir`目录不存在时，则将返回`不存在目录`的错误。
* 使用`requestError(QNetworkReply::NetworkError, const QString &)`信号处理错误。

<a name="获取空间使用量情况"></a>
### 获取空间使用量情况
```
QUpYun *upyun = new QUpYun(BucketName, Operator, Password, parent);
connect(upyun, &QUpYun::requestBucketUsageFinished, [=] (qulonglong usage) {
	...
});

upyun->bucketUsage();
```

##### 其他说明
* 使用量的单位为 `byte`，比如`1M`的使用量将以`1048576`这样的数字返回。
* 使用`requestError(QNetworkReply::NetworkError, const QString &)`信号处理错误。