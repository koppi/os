/****************************************************************************
** Meta object code from reading C++ file 'qinputdevice.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/kernel/qinputdevice.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qinputdevice.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN12QInputDeviceE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN12QInputDeviceE = QtMocHelpers::stringData(
    "QInputDevice",
    "availableVirtualGeometryChanged",
    "",
    "area",
    "name",
    "type",
    "DeviceType",
    "capabilities",
    "Capabilities",
    "systemId",
    "seatName",
    "availableVirtualGeometry",
    "DeviceTypes",
    "Unknown",
    "Mouse",
    "TouchScreen",
    "TouchPad",
    "Puck",
    "Stylus",
    "Airbrush",
    "Keyboard",
    "AllDevices",
    "Capability",
    "None",
    "Position",
    "Area",
    "Pressure",
    "Velocity",
    "NormalizedPosition",
    "MouseEmulation",
    "PixelScroll",
    "Scroll",
    "Hover",
    "Rotation",
    "XTilt",
    "YTilt",
    "TangentialPressure",
    "ZPosition",
    "All"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN12QInputDeviceE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       6,   23, // properties
       2,   53, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   20,    2, 0x06,    9 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QRect,    3,

 // properties: name, type, flags, notifyId, revision
       4, QMetaType::QString, 0x00015401, uint(-1), 0,
       5, 0x80000000 | 6, 0x00015409, uint(-1), 0,
       7, 0x80000000 | 8, 0x00015009, uint(-1), 0,
       9, QMetaType::LongLong, 0x00015401, uint(-1), 0,
      10, QMetaType::QString, 0x00015401, uint(-1), 0,
      11, QMetaType::QRect, 0x00015001, uint(0), 0,

 // enums: name, alias, flags, count, data
      12,    6, 0x3,    9,   63,
       8,   22, 0x3,   16,   81,

 // enum data: key, value
      13, uint(QInputDevice::DeviceType::Unknown),
      14, uint(QInputDevice::DeviceType::Mouse),
      15, uint(QInputDevice::DeviceType::TouchScreen),
      16, uint(QInputDevice::DeviceType::TouchPad),
      17, uint(QInputDevice::DeviceType::Puck),
      18, uint(QInputDevice::DeviceType::Stylus),
      19, uint(QInputDevice::DeviceType::Airbrush),
      20, uint(QInputDevice::DeviceType::Keyboard),
      21, uint(QInputDevice::DeviceType::AllDevices),
      23, uint(QInputDevice::Capability::None),
      24, uint(QInputDevice::Capability::Position),
      25, uint(QInputDevice::Capability::Area),
      26, uint(QInputDevice::Capability::Pressure),
      27, uint(QInputDevice::Capability::Velocity),
      28, uint(QInputDevice::Capability::NormalizedPosition),
      29, uint(QInputDevice::Capability::MouseEmulation),
      30, uint(QInputDevice::Capability::PixelScroll),
      31, uint(QInputDevice::Capability::Scroll),
      32, uint(QInputDevice::Capability::Hover),
      33, uint(QInputDevice::Capability::Rotation),
      34, uint(QInputDevice::Capability::XTilt),
      35, uint(QInputDevice::Capability::YTilt),
      36, uint(QInputDevice::Capability::TangentialPressure),
      37, uint(QInputDevice::Capability::ZPosition),
      38, uint(QInputDevice::Capability::All),

       0        // eod
};

Q_CONSTINIT const QMetaObject QInputDevice::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN12QInputDeviceE.offsetsAndSizes,
    qt_meta_data_ZN12QInputDeviceE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN12QInputDeviceE_t,
        // property 'name'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'type'
        QtPrivate::TypeAndForceComplete<DeviceType, std::true_type>,
        // property 'capabilities'
        QtPrivate::TypeAndForceComplete<Capabilities, std::true_type>,
        // property 'systemId'
        QtPrivate::TypeAndForceComplete<qint64, std::true_type>,
        // property 'seatName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'availableVirtualGeometry'
        QtPrivate::TypeAndForceComplete<QRect, std::true_type>,
        // enum 'DeviceTypes'
        QtPrivate::TypeAndForceComplete<QInputDevice::DeviceTypes, std::true_type>,
        // enum 'Capabilities'
        QtPrivate::TypeAndForceComplete<QInputDevice::Capabilities, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QInputDevice, std::true_type>,
        // method 'availableVirtualGeometryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QRect, std::false_type>
    >,
    nullptr
} };

void QInputDevice::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QInputDevice *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->availableVirtualGeometryChanged((*reinterpret_cast< std::add_pointer_t<QRect>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QInputDevice::*)(QRect );
            if (_q_method_type _q_method = &QInputDevice::availableVirtualGeometryChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->name(); break;
        case 1: *reinterpret_cast< DeviceType*>(_v) = _t->type(); break;
        case 2: *reinterpret_cast<int*>(_v) = QFlag(_t->capabilities()); break;
        case 3: *reinterpret_cast< qint64*>(_v) = _t->systemId(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->seatName(); break;
        case 5: *reinterpret_cast< QRect*>(_v) = _t->availableVirtualGeometry(); break;
        default: break;
        }
    }
}

const QMetaObject *QInputDevice::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QInputDevice::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN12QInputDeviceE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int QInputDevice::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
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
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void QInputDevice::availableVirtualGeometryChanged(QRect _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
