/****************************************************************************
** Meta object code from reading C++ file 'qwidget_p.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/kernel/qwidget_p.h"
#include <QtGui/qscreen.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwidget_p.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14QWidgetPrivateE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14QWidgetPrivateE = QtMocHelpers::stringData(
    "QWidgetPrivate",
    "DrawWidgetFlags",
    "DrawWidgetFlag",
    "DrawAsRoot",
    "DrawPaintOnScreen",
    "DrawRecursive",
    "DrawInvisible",
    "DontSubtractOpaqueChildren",
    "DontDrawOpaqueChildren",
    "DontDrawNativeChildren",
    "DontSetCompositionMode",
    "UseEffectRegionBounds",
    "Direction",
    "DirectionNorth",
    "DirectionEast",
    "DirectionSouth",
    "DirectionWest",
    "CloseMode",
    "CloseNoEvent",
    "CloseWithEvent",
    "CloseWithSpontaneousEvent"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14QWidgetPrivateE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       3,   14, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // enums: name, alias, flags, count, data
       1,    2, 0x1,    9,   29,
      12,   12, 0x0,    4,   47,
      17,   17, 0x0,    3,   55,

 // enum data: key, value
       3, uint(QWidgetPrivate::DrawAsRoot),
       4, uint(QWidgetPrivate::DrawPaintOnScreen),
       5, uint(QWidgetPrivate::DrawRecursive),
       6, uint(QWidgetPrivate::DrawInvisible),
       7, uint(QWidgetPrivate::DontSubtractOpaqueChildren),
       8, uint(QWidgetPrivate::DontDrawOpaqueChildren),
       9, uint(QWidgetPrivate::DontDrawNativeChildren),
      10, uint(QWidgetPrivate::DontSetCompositionMode),
      11, uint(QWidgetPrivate::UseEffectRegionBounds),
      13, uint(QWidgetPrivate::DirectionNorth),
      14, uint(QWidgetPrivate::DirectionEast),
      15, uint(QWidgetPrivate::DirectionSouth),
      16, uint(QWidgetPrivate::DirectionWest),
      18, uint(QWidgetPrivate::CloseNoEvent),
      19, uint(QWidgetPrivate::CloseWithEvent),
      20, uint(QWidgetPrivate::CloseWithSpontaneousEvent),

       0        // eod
};

Q_CONSTINIT const QMetaObject QWidgetPrivate::staticMetaObject = { {
    QtPrivate::MetaObjectForType<QObjectPrivate>::value,
    qt_meta_stringdata_ZN14QWidgetPrivateE.offsetsAndSizes,
    qt_meta_data_ZN14QWidgetPrivateE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14QWidgetPrivateE_t,
        // enum 'DrawWidgetFlags'
        QtPrivate::TypeAndForceComplete<QWidgetPrivate::DrawWidgetFlags, std::true_type>,
        // enum 'Direction'
        QtPrivate::TypeAndForceComplete<QWidgetPrivate::Direction, std::true_type>,
        // enum 'CloseMode'
        QtPrivate::TypeAndForceComplete<QWidgetPrivate::CloseMode, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QWidgetPrivate, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
