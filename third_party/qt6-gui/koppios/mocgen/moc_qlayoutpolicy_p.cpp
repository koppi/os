/****************************************************************************
** Meta object code from reading C++ file 'qlayoutpolicy_p.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/util/qlayoutpolicy_p.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qlayoutpolicy_p.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13QLayoutPolicyE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN13QLayoutPolicyE = QtMocHelpers::stringData(
    "QLayoutPolicy",
    "Policy",
    "PolicyFlag",
    "GrowFlag",
    "ExpandFlag",
    "ShrinkFlag",
    "IgnoreFlag"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN13QLayoutPolicyE[] = {

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
       1,    2, 0x1,    4,   19,

 // enum data: key, value
       3, uint(QLayoutPolicy::GrowFlag),
       4, uint(QLayoutPolicy::ExpandFlag),
       5, uint(QLayoutPolicy::ShrinkFlag),
       6, uint(QLayoutPolicy::IgnoreFlag),

       0        // eod
};

Q_CONSTINIT const QMetaObject QLayoutPolicy::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN13QLayoutPolicyE.offsetsAndSizes,
    qt_meta_data_ZN13QLayoutPolicyE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN13QLayoutPolicyE_t,
        // enum 'Policy'
        QtPrivate::TypeAndForceComplete<QLayoutPolicy::Policy, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QLayoutPolicy, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
