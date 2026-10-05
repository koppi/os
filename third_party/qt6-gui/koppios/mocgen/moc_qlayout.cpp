/****************************************************************************
** Meta object code from reading C++ file 'qlayout.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/kernel/qlayout.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qlayout.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7QLayoutE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN7QLayoutE = QtMocHelpers::stringData(
    "QLayout",
    "spacing",
    "contentsMargins",
    "QMargins",
    "sizeConstraint",
    "SizeConstraint",
    "SetDefaultConstraint",
    "SetNoConstraint",
    "SetMinimumSize",
    "SetFixedSize",
    "SetMaximumSize",
    "SetMinAndMaxSize"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN7QLayoutE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       3,   14, // properties
       1,   29, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, QMetaType::Int, 0x00015103, uint(-1), 0,
       2, 0x80000000 | 3, 0x0001510f, uint(-1), 0,
       4, 0x80000000 | 5, 0x0001510b, uint(-1), 0,

 // enums: name, alias, flags, count, data
       5,    5, 0x0,    6,   34,

 // enum data: key, value
       6, uint(QLayout::SetDefaultConstraint),
       7, uint(QLayout::SetNoConstraint),
       8, uint(QLayout::SetMinimumSize),
       9, uint(QLayout::SetFixedSize),
      10, uint(QLayout::SetMaximumSize),
      11, uint(QLayout::SetMinAndMaxSize),

       0        // eod
};

Q_CONSTINIT const QMetaObject QLayout::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN7QLayoutE.offsetsAndSizes,
    qt_meta_data_ZN7QLayoutE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN7QLayoutE_t,
        // property 'spacing'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'contentsMargins'
        QtPrivate::TypeAndForceComplete<QMargins, std::true_type>,
        // property 'sizeConstraint'
        QtPrivate::TypeAndForceComplete<SizeConstraint, std::true_type>,
        // enum 'SizeConstraint'
        QtPrivate::TypeAndForceComplete<QLayout::SizeConstraint, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QLayout, std::true_type>
    >,
    nullptr
} };

void QLayout::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QLayout *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = _t->spacing(); break;
        case 1: *reinterpret_cast< QMargins*>(_v) = _t->contentsMargins(); break;
        case 2: *reinterpret_cast< SizeConstraint*>(_v) = _t->sizeConstraint(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setSpacing(*reinterpret_cast< int*>(_v)); break;
        case 1: _t->setContentsMargins(*reinterpret_cast< QMargins*>(_v)); break;
        case 2: _t->setSizeConstraint(*reinterpret_cast< SizeConstraint*>(_v)); break;
        default: break;
        }
    }
if (_c == QMetaObject::ResetProperty) {
        switch (_id) {
        case 1: _t->unsetContentsMargins(); break;
        default: break;
        }
    }
}

const QMetaObject *QLayout::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QLayout::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN7QLayoutE.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "QLayoutItem"))
        return static_cast< QLayoutItem*>(this);
    return QObject::qt_metacast(_clname);
}

int QLayout::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}
QT_WARNING_POP
