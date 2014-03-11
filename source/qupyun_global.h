#ifndef QUPYUN_GLOBAL_H
#define QUPYUN_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(QUPYUN_LIBRARY)
#  define QUPYUNSHARED_EXPORT Q_DECL_EXPORT
#else
#  define QUPYUNSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // QUPYUN_GLOBAL_H
