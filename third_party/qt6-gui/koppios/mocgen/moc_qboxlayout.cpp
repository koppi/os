/****************************************************************************
** Meta object code from reading C++ file 'qboxlayout.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/kernel/qboxlayout.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qboxlayout.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10QBoxLayoutE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10QBoxLayoutE = QtMocHelpers::stringData(
    "QBoxLayout"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10QBoxLayoutE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

Q_CONSTINIT const QMetaObject QBoxLayout::staticMetaObject = { {
    QMetaObject::SuperData::link<QLayout::staticMetaObject>(),
    qt_meta_stringdata_ZN10QBoxLayoutE.offsetsAndSizes,
    qt_meta_data_ZN10QBoxLayoutE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10QBoxLayoutE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QBoxLayout, std::true_type>
    >,
    nullptr
} };

void QBoxLayout::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QBoxLayout *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *QBoxLayout::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QBoxLayout::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN10QBoxLayoutE.stringdata0))
        return static_cast<void*>(this);
    return QLayout::qt_metacast(_clname);
}

int QBoxLayout::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QLayout::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN11QHBoxLayoutE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN11QHBoxLayoutE = QtMocHelpers::stringData(
    "QHBoxLayout"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN11QHBoxLayoutE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

Q_CONSTINIT const QMetaObject QHBoxLayout::staticMetaObject = { {
    QMetaObject::SuperData::link<QBoxLayout::staticMetaObject>(),
    qt_meta_stringdata_ZN11QHBoxLayoutE.offsetsAndSizes,
    qt_meta_data_ZN11QHBoxLayoutE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN11QHBoxLayoutE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QHBoxLayout, std::true_type>
    >,
    nullptr
} };

void QHBoxLayout::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QHBoxLayout *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *QHBoxLayout::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QHBoxLayout::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN11QHBoxLayoutE.stringdata0))
        return static_cast<void*>(this);
    return QBoxLayout::qt_metacast(_clname);
}

int QHBoxLayout::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QBoxLayout::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN11QVBoxLayoutE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN11QVBoxLayoutE = QtMocHelpers::stringData(
    "QVBoxLayout"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN11QVBoxLayoutE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

Q_CONSTINIT const QMetaObject QVBoxLayout::staticMetaObject = { {
    QMetaObject::SuperData::link<QBoxLayout::staticMetaObject>(),
    qt_meta_stringdata_ZN11QVBoxLayoutE.offsetsAndSizes,
    qt_meta_data_ZN11QVBoxLayoutE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN11QVBoxLayoutE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QVBoxLayout, std::true_type>
    >,
    nullptr
} };

void QVBoxLayout::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QVBoxLayout *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *QVBoxLayout::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QVBoxLayout::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN11QVBoxLayoutE.stringdata0))
        return static_cast<void*>(this);
    return QBoxLayout::qt_metacast(_clname);
}

int QVBoxLayout::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QBoxLayout::qt_metacall(_c, _id, _a);
    return _id;
}
QT_WARNING_POP
