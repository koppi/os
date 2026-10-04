/****************************************************************************
** Meta object code from reading C++ file 'qsurface.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/kernel/qsurface.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qsurface.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN8QSurfaceE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN8QSurfaceE = QtMocHelpers::stringData(
    "QSurface",
    "SurfaceClass",
    "Window",
    "Offscreen",
    "SurfaceType",
    "RasterSurface",
    "OpenGLSurface",
    "RasterGLSurface",
    "OpenVGSurface",
    "VulkanSurface",
    "MetalSurface",
    "Direct3DSurface"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN8QSurfaceE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       2,   14, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // enums: name, alias, flags, count, data
       1,    1, 0x0,    2,   24,
       4,    4, 0x0,    7,   28,

 // enum data: key, value
       2, uint(QSurface::Window),
       3, uint(QSurface::Offscreen),
       5, uint(QSurface::RasterSurface),
       6, uint(QSurface::OpenGLSurface),
       7, uint(QSurface::RasterGLSurface),
       8, uint(QSurface::OpenVGSurface),
       9, uint(QSurface::VulkanSurface),
      10, uint(QSurface::MetalSurface),
      11, uint(QSurface::Direct3DSurface),

       0        // eod
};

Q_CONSTINIT const QMetaObject QSurface::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN8QSurfaceE.offsetsAndSizes,
    qt_meta_data_ZN8QSurfaceE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN8QSurfaceE_t,
        // enum 'SurfaceClass'
        QtPrivate::TypeAndForceComplete<QSurface::SurfaceClass, std::true_type>,
        // enum 'SurfaceType'
        QtPrivate::TypeAndForceComplete<QSurface::SurfaceType, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QSurface, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
