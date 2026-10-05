/****************************************************************************
** Meta object code from reading C++ file 'qslider.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qslider.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qslider.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7QSliderE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN7QSliderE = QtMocHelpers::stringData(
    "QSlider",
    "tickPosition",
    "TickPosition",
    "tickInterval",
    "NoTicks",
    "TicksAbove",
    "TicksLeft",
    "TicksBelow",
    "TicksRight",
    "TicksBothSides"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN7QSliderE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       2,   14, // properties
       1,   24, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, 0x80000000 | 2, 0x0001510b, uint(-1), 0,
       3, QMetaType::Int, 0x00015103, uint(-1), 0,

 // enums: name, alias, flags, count, data
       2,    2, 0x0,    6,   29,

 // enum data: key, value
       4, uint(QSlider::NoTicks),
       5, uint(QSlider::TicksAbove),
       6, uint(QSlider::TicksLeft),
       7, uint(QSlider::TicksBelow),
       8, uint(QSlider::TicksRight),
       9, uint(QSlider::TicksBothSides),

       0        // eod
};

Q_CONSTINIT const QMetaObject QSlider::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractSlider::staticMetaObject>(),
    qt_meta_stringdata_ZN7QSliderE.offsetsAndSizes,
    qt_meta_data_ZN7QSliderE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN7QSliderE_t,
        // property 'tickPosition'
        QtPrivate::TypeAndForceComplete<TickPosition, std::true_type>,
        // property 'tickInterval'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // enum 'TickPosition'
        QtPrivate::TypeAndForceComplete<QSlider::TickPosition, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QSlider, std::true_type>
    >,
    nullptr
} };

void QSlider::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QSlider *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< TickPosition*>(_v) = _t->tickPosition(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->tickInterval(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setTickPosition(*reinterpret_cast< TickPosition*>(_v)); break;
        case 1: _t->setTickInterval(*reinterpret_cast< int*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QSlider::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QSlider::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN7QSliderE.stringdata0))
        return static_cast<void*>(this);
    return QAbstractSlider::qt_metacast(_clname);
}

int QSlider::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractSlider::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}
QT_WARNING_POP
