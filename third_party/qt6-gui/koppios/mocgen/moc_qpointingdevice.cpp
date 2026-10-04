/****************************************************************************
** Meta object code from reading C++ file 'qpointingdevice.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/kernel/qpointingdevice.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qpointingdevice.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN23QPointingDeviceUniqueIdE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN23QPointingDeviceUniqueIdE = QtMocHelpers::stringData(
    "QPointingDeviceUniqueId",
    "numericId"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN23QPointingDeviceUniqueIdE[] = {

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
       1, QMetaType::LongLong, 0x00015401, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QPointingDeviceUniqueId::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN23QPointingDeviceUniqueIdE.offsetsAndSizes,
    qt_meta_data_ZN23QPointingDeviceUniqueIdE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN23QPointingDeviceUniqueIdE_t,
        // property 'numericId'
        QtPrivate::TypeAndForceComplete<qint64, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QPointingDeviceUniqueId, std::true_type>
    >,
    nullptr
} };

void QPointingDeviceUniqueId::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = reinterpret_cast<QPointingDeviceUniqueId *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< qint64*>(_v) = _t->numericId(); break;
        default: break;
        }
    }
}
namespace {
struct qt_meta_tag_ZN15QPointingDeviceE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN15QPointingDeviceE = QtMocHelpers::stringData(
    "QPointingDevice",
    "grabChanged",
    "",
    "grabber",
    "GrabTransition",
    "transition",
    "const QPointerEvent*",
    "event",
    "QEventPoint",
    "point",
    "pointerType",
    "PointerType",
    "maximumPoints",
    "buttonCount",
    "uniqueId",
    "QPointingDeviceUniqueId",
    "PointerTypes",
    "Unknown",
    "Generic",
    "Finger",
    "Pen",
    "Eraser",
    "Cursor",
    "AllPointerTypes",
    "GrabPassive",
    "UngrabPassive",
    "CancelGrabPassive",
    "OverrideGrabPassive",
    "GrabExclusive",
    "UngrabExclusive",
    "CancelGrabExclusive"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN15QPointingDeviceE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       4,   29, // properties
       2,   49, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    4,   20,    2, 0x106,    7 /* Public | MethodIsConst  */,

 // signals: parameters
    QMetaType::Void, QMetaType::QObjectStar, 0x80000000 | 4, 0x80000000 | 6, 0x80000000 | 8,    3,    5,    7,    9,

 // properties: name, type, flags, notifyId, revision
      10, 0x80000000 | 11, 0x00015409, uint(-1), 0,
      12, QMetaType::Int, 0x00015401, uint(-1), 0,
      13, QMetaType::Int, 0x00015401, uint(-1), 0,
      14, 0x80000000 | 15, 0x00015409, uint(-1), 0,

 // enums: name, alias, flags, count, data
      16,   11, 0x3,    7,   59,
       4,    4, 0x0,    7,   73,

 // enum data: key, value
      17, uint(QPointingDevice::PointerType::Unknown),
      18, uint(QPointingDevice::PointerType::Generic),
      19, uint(QPointingDevice::PointerType::Finger),
      20, uint(QPointingDevice::PointerType::Pen),
      21, uint(QPointingDevice::PointerType::Eraser),
      22, uint(QPointingDevice::PointerType::Cursor),
      23, uint(QPointingDevice::PointerType::AllPointerTypes),
      24, uint(QPointingDevice::GrabPassive),
      25, uint(QPointingDevice::UngrabPassive),
      26, uint(QPointingDevice::CancelGrabPassive),
      27, uint(QPointingDevice::OverrideGrabPassive),
      28, uint(QPointingDevice::GrabExclusive),
      29, uint(QPointingDevice::UngrabExclusive),
      30, uint(QPointingDevice::CancelGrabExclusive),

       0        // eod
};

Q_CONSTINIT const QMetaObject QPointingDevice::staticMetaObject = { {
    QMetaObject::SuperData::link<QInputDevice::staticMetaObject>(),
    qt_meta_stringdata_ZN15QPointingDeviceE.offsetsAndSizes,
    qt_meta_data_ZN15QPointingDeviceE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN15QPointingDeviceE_t,
        // property 'pointerType'
        QtPrivate::TypeAndForceComplete<PointerType, std::true_type>,
        // property 'maximumPoints'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'buttonCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'uniqueId'
        QtPrivate::TypeAndForceComplete<QPointingDeviceUniqueId, std::true_type>,
        // enum 'PointerTypes'
        QtPrivate::TypeAndForceComplete<QPointingDevice::PointerTypes, std::true_type>,
        // enum 'GrabTransition'
        QtPrivate::TypeAndForceComplete<QPointingDevice::GrabTransition, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QPointingDevice, std::true_type>,
        // method 'grabChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QObject *, std::false_type>,
        QtPrivate::TypeAndForceComplete<GrabTransition, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPointerEvent *, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QEventPoint &, std::false_type>
    >,
    nullptr
} };

void QPointingDevice::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QPointingDevice *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->grabChanged((*reinterpret_cast< std::add_pointer_t<QObject*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<GrabTransition>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<const QPointerEvent*>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QEventPoint>>(_a[4]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QPointingDevice::*)(QObject * , GrabTransition , const QPointerEvent * , const QEventPoint & ) const;
            if (_q_method_type _q_method = &QPointingDevice::grabChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< PointerType*>(_v) = _t->pointerType(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->maximumPoints(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->buttonCount(); break;
        case 3: *reinterpret_cast< QPointingDeviceUniqueId*>(_v) = _t->uniqueId(); break;
        default: break;
        }
    }
}

const QMetaObject *QPointingDevice::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QPointingDevice::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN15QPointingDeviceE.stringdata0))
        return static_cast<void*>(this);
    return QInputDevice::qt_metacast(_clname);
}

int QPointingDevice::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QInputDevice::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 1;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void QPointingDevice::grabChanged(QObject * _t1, GrabTransition _t2, const QPointerEvent * _t3, const QEventPoint & _t4)const
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(const_cast< QPointingDevice *>(this), &staticMetaObject, 0, _a);
}
QT_WARNING_POP
