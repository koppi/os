/****************************************************************************
** Meta object code from reading C++ file 'qimage.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/image/qimage.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qimage.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN6QImageE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN6QImageE = QtMocHelpers::stringData(
    "QImage",
    "Format",
    "Format_Invalid",
    "Format_Mono",
    "Format_MonoLSB",
    "Format_Indexed8",
    "Format_RGB32",
    "Format_ARGB32",
    "Format_ARGB32_Premultiplied",
    "Format_RGB16",
    "Format_ARGB8565_Premultiplied",
    "Format_RGB666",
    "Format_ARGB6666_Premultiplied",
    "Format_RGB555",
    "Format_ARGB8555_Premultiplied",
    "Format_RGB888",
    "Format_RGB444",
    "Format_ARGB4444_Premultiplied",
    "Format_RGBX8888",
    "Format_RGBA8888",
    "Format_RGBA8888_Premultiplied",
    "Format_BGR30",
    "Format_A2BGR30_Premultiplied",
    "Format_RGB30",
    "Format_A2RGB30_Premultiplied",
    "Format_Alpha8",
    "Format_Grayscale8",
    "Format_RGBX64",
    "Format_RGBA64",
    "Format_RGBA64_Premultiplied",
    "Format_Grayscale16",
    "Format_BGR888",
    "Format_RGBX16FPx4",
    "Format_RGBA16FPx4",
    "Format_RGBA16FPx4_Premultiplied",
    "Format_RGBX32FPx4",
    "Format_RGBA32FPx4",
    "Format_RGBA32FPx4_Premultiplied",
    "Format_CMYK8888",
    "NImageFormats"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN6QImageE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       1,   14, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // enums: name, alias, flags, count, data
       1,    1, 0x0,   38,   19,

 // enum data: key, value
       2, uint(QImage::Format_Invalid),
       3, uint(QImage::Format_Mono),
       4, uint(QImage::Format_MonoLSB),
       5, uint(QImage::Format_Indexed8),
       6, uint(QImage::Format_RGB32),
       7, uint(QImage::Format_ARGB32),
       8, uint(QImage::Format_ARGB32_Premultiplied),
       9, uint(QImage::Format_RGB16),
      10, uint(QImage::Format_ARGB8565_Premultiplied),
      11, uint(QImage::Format_RGB666),
      12, uint(QImage::Format_ARGB6666_Premultiplied),
      13, uint(QImage::Format_RGB555),
      14, uint(QImage::Format_ARGB8555_Premultiplied),
      15, uint(QImage::Format_RGB888),
      16, uint(QImage::Format_RGB444),
      17, uint(QImage::Format_ARGB4444_Premultiplied),
      18, uint(QImage::Format_RGBX8888),
      19, uint(QImage::Format_RGBA8888),
      20, uint(QImage::Format_RGBA8888_Premultiplied),
      21, uint(QImage::Format_BGR30),
      22, uint(QImage::Format_A2BGR30_Premultiplied),
      23, uint(QImage::Format_RGB30),
      24, uint(QImage::Format_A2RGB30_Premultiplied),
      25, uint(QImage::Format_Alpha8),
      26, uint(QImage::Format_Grayscale8),
      27, uint(QImage::Format_RGBX64),
      28, uint(QImage::Format_RGBA64),
      29, uint(QImage::Format_RGBA64_Premultiplied),
      30, uint(QImage::Format_Grayscale16),
      31, uint(QImage::Format_BGR888),
      32, uint(QImage::Format_RGBX16FPx4),
      33, uint(QImage::Format_RGBA16FPx4),
      34, uint(QImage::Format_RGBA16FPx4_Premultiplied),
      35, uint(QImage::Format_RGBX32FPx4),
      36, uint(QImage::Format_RGBA32FPx4),
      37, uint(QImage::Format_RGBA32FPx4_Premultiplied),
      38, uint(QImage::Format_CMYK8888),
      39, uint(QImage::NImageFormats),

       0        // eod
};

Q_CONSTINIT const QMetaObject QImage::staticMetaObject = { {
    QtPrivate::MetaObjectForType<QPaintDevice>::value,
    qt_meta_stringdata_ZN6QImageE.offsetsAndSizes,
    qt_meta_data_ZN6QImageE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN6QImageE_t,
        // enum 'Format'
        QtPrivate::TypeAndForceComplete<QImage::Format, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QImage, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
