/****************************************************************************
** Meta object code from reading C++ file 'qplatformgraphicsbuffer.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/kernel/qplatformgraphicsbuffer.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qplatformgraphicsbuffer.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN23QPlatformGraphicsBufferE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN23QPlatformGraphicsBufferE = QtMocHelpers::stringData(
    "QPlatformGraphicsBuffer",
    "unlocked",
    "",
    "AccessTypes",
    "previousAccessTypes",
    "AccessType",
    "None",
    "SWReadAccess",
    "SWWriteAccess",
    "TextureAccess",
    "HWCompositor",
    "Origin",
    "OriginBottomLeft",
    "OriginTopLeft"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN23QPlatformGraphicsBufferE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       0,    0, // properties
       2,   23, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   20,    2, 0x06,    3 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,

 // enums: name, alias, flags, count, data
       5,    5, 0x0,    5,   33,
      11,   11, 0x0,    2,   43,

 // enum data: key, value
       6, uint(QPlatformGraphicsBuffer::None),
       7, uint(QPlatformGraphicsBuffer::SWReadAccess),
       8, uint(QPlatformGraphicsBuffer::SWWriteAccess),
       9, uint(QPlatformGraphicsBuffer::TextureAccess),
      10, uint(QPlatformGraphicsBuffer::HWCompositor),
      12, uint(QPlatformGraphicsBuffer::OriginBottomLeft),
      13, uint(QPlatformGraphicsBuffer::OriginTopLeft),

       0        // eod
};

Q_CONSTINIT const QMetaObject QPlatformGraphicsBuffer::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN23QPlatformGraphicsBufferE.offsetsAndSizes,
    qt_meta_data_ZN23QPlatformGraphicsBufferE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN23QPlatformGraphicsBufferE_t,
        // enum 'AccessType'
        QtPrivate::TypeAndForceComplete<QPlatformGraphicsBuffer::AccessType, std::true_type>,
        // enum 'Origin'
        QtPrivate::TypeAndForceComplete<QPlatformGraphicsBuffer::Origin, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QPlatformGraphicsBuffer, std::true_type>,
        // method 'unlocked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<AccessTypes, std::false_type>
    >,
    nullptr
} };

void QPlatformGraphicsBuffer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QPlatformGraphicsBuffer *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->unlocked((*reinterpret_cast< std::add_pointer_t<AccessTypes>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QPlatformGraphicsBuffer::*)(AccessTypes );
            if (_q_method_type _q_method = &QPlatformGraphicsBuffer::unlocked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *QPlatformGraphicsBuffer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QPlatformGraphicsBuffer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN23QPlatformGraphicsBufferE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int QPlatformGraphicsBuffer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
    return _id;
}

// SIGNAL 0
void QPlatformGraphicsBuffer::unlocked(AccessTypes _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
