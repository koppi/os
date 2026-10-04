/****************************************************************************
** Meta object code from reading C++ file 'qcborcommon.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/corelib/serialization/qcborcommon.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qcborcommon.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10QCborErrorE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10QCborErrorE = QtMocHelpers::stringData(
    "QCborError",
    "Code",
    "UnknownError",
    "AdvancePastEnd",
    "InputOutputError",
    "GarbageAtEnd",
    "EndOfFile",
    "UnexpectedBreak",
    "UnknownType",
    "IllegalType",
    "IllegalNumber",
    "IllegalSimpleType",
    "InvalidUtf8String",
    "DataTooLarge",
    "NestingTooDeep",
    "UnsupportedType",
    "NoError"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10QCborErrorE[] = {

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
       1,    1, 0x0,   15,   19,

 // enum data: key, value
       2, uint(QCborError::UnknownError),
       3, uint(QCborError::AdvancePastEnd),
       4, uint(QCborError::InputOutputError),
       5, uint(QCborError::GarbageAtEnd),
       6, uint(QCborError::EndOfFile),
       7, uint(QCborError::UnexpectedBreak),
       8, uint(QCborError::UnknownType),
       9, uint(QCborError::IllegalType),
      10, uint(QCborError::IllegalNumber),
      11, uint(QCborError::IllegalSimpleType),
      12, uint(QCborError::InvalidUtf8String),
      13, uint(QCborError::DataTooLarge),
      14, uint(QCborError::NestingTooDeep),
      15, uint(QCborError::UnsupportedType),
      16, uint(QCborError::NoError),

       0        // eod
};

Q_CONSTINIT const QMetaObject QCborError::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN10QCborErrorE.offsetsAndSizes,
    qt_meta_data_ZN10QCborErrorE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10QCborErrorE_t,
        // enum 'Code'
        QtPrivate::TypeAndForceComplete<QCborError::Code, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QCborError, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
