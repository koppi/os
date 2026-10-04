/****************************************************************************
** Meta object code from reading C++ file 'qevent.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/kernel/qevent.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qevent.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13QPointerEventE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN13QPointerEventE = QtMocHelpers::stringData(
    "QPointerEvent"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN13QPointerEventE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

       0        // eod
};

Q_CONSTINIT const QMetaObject QPointerEvent::staticMetaObject = { {
    QtPrivate::MetaObjectForType<QInputEvent>::value,
    qt_meta_stringdata_ZN13QPointerEventE.offsetsAndSizes,
    qt_meta_data_ZN13QPointerEventE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN13QPointerEventE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QPointerEvent, std::true_type>
    >,
    nullptr
} };

namespace {
struct qt_meta_tag_ZN17QSinglePointEventE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN17QSinglePointEventE = QtMocHelpers::stringData(
    "QSinglePointEvent",
    "exclusivePointGrabber"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN17QSinglePointEventE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       1,   14, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, QMetaType::QObjectStar, 0x00015103, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QSinglePointEvent::staticMetaObject = { {
    QtPrivate::MetaObjectForType<QPointerEvent>::value,
    qt_meta_stringdata_ZN17QSinglePointEventE.offsetsAndSizes,
    qt_meta_data_ZN17QSinglePointEventE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN17QSinglePointEventE_t,
        // property 'exclusivePointGrabber'
        QtPrivate::TypeAndForceComplete<QObject*, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QSinglePointEvent, std::true_type>
    >,
    nullptr
} };

void QSinglePointEvent::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = reinterpret_cast<QSinglePointEvent *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QObject**>(_v) = _t->exclusivePointGrabber(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setExclusivePointGrabber(*reinterpret_cast< QObject**>(_v)); break;
        default: break;
        }
    }
}
namespace {
struct qt_meta_tag_ZN11QWheelEventE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN11QWheelEventE = QtMocHelpers::stringData(
    "QWheelEvent",
    "device",
    "const QPointingDevice*",
    "pixelDelta",
    "angleDelta",
    "phase",
    "Qt::ScrollPhase",
    "inverted"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN11QWheelEventE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       5,   14, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, 0x80000000 | 2, 0x00015009, uint(-1), 0,
       3, QMetaType::QPoint, 0x00015001, uint(-1), 0,
       4, QMetaType::QPoint, 0x00015001, uint(-1), 0,
       5, 0x80000000 | 6, 0x00015009, uint(-1), 0,
       7, QMetaType::Bool, 0x00015001, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QWheelEvent::staticMetaObject = { {
    QtPrivate::MetaObjectForType<QSinglePointEvent>::value,
    qt_meta_stringdata_ZN11QWheelEventE.offsetsAndSizes,
    qt_meta_data_ZN11QWheelEventE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN11QWheelEventE_t,
        // property 'device'
        QtPrivate::TypeAndForceComplete<const QPointingDevice*, std::true_type>,
        // property 'pixelDelta'
        QtPrivate::TypeAndForceComplete<QPoint, std::true_type>,
        // property 'angleDelta'
        QtPrivate::TypeAndForceComplete<QPoint, std::true_type>,
        // property 'phase'
        QtPrivate::TypeAndForceComplete<Qt::ScrollPhase, std::true_type>,
        // property 'inverted'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QWheelEvent, std::true_type>
    >,
    nullptr
} };

void QWheelEvent::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = reinterpret_cast<QWheelEvent *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< const QPointingDevice**>(_v) = _t->pointingDevice(); break;
        case 1: *reinterpret_cast< QPoint*>(_v) = _t->pixelDelta(); break;
        case 2: *reinterpret_cast< QPoint*>(_v) = _t->angleDelta(); break;
        case 3: *reinterpret_cast< Qt::ScrollPhase*>(_v) = _t->phase(); break;
        case 4: *reinterpret_cast< bool*>(_v) = _t->inverted(); break;
        default: break;
        }
    }
}
QT_WARNING_POP
