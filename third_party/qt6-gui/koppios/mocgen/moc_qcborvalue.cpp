/****************************************************************************
** Meta object code from reading C++ file 'qcborvalue.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/corelib/serialization/qcborvalue.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qcborvalue.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10QCborValueE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10QCborValueE = QtMocHelpers::stringData(
    "QCborValue",
    "Type",
    "Integer",
    "ByteArray",
    "String",
    "Array",
    "Map",
    "Tag",
    "SimpleType",
    "False",
    "True",
    "Null",
    "Undefined",
    "Double",
    "DateTime",
    "Url",
    "RegularExpression",
    "Uuid",
    "Invalid"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10QCborValueE[] = {

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
       1,    1, 0x0,   17,   19,

 // enum data: key, value
       2, uint(QCborValue::Integer),
       3, uint(QCborValue::ByteArray),
       4, uint(QCborValue::String),
       5, uint(QCborValue::Array),
       6, uint(QCborValue::Map),
       7, uint(QCborValue::Tag),
       8, uint(QCborValue::SimpleType),
       9, uint(QCborValue::False),
      10, uint(QCborValue::True),
      11, uint(QCborValue::Null),
      12, uint(QCborValue::Undefined),
      13, uint(QCborValue::Double),
      14, uint(QCborValue::DateTime),
      15, uint(QCborValue::Url),
      16, uint(QCborValue::RegularExpression),
      17, uint(QCborValue::Uuid),
      18, uint(QCborValue::Invalid),

       0        // eod
};

Q_CONSTINIT const QMetaObject QCborValue::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN10QCborValueE.offsetsAndSizes,
    qt_meta_data_ZN10QCborValueE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10QCborValueE_t,
        // enum 'Type'
        QtPrivate::TypeAndForceComplete<QCborValue::Type, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QCborValue, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
