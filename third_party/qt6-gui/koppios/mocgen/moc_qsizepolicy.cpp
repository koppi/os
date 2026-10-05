/****************************************************************************
** Meta object code from reading C++ file 'qsizepolicy.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/kernel/qsizepolicy.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qsizepolicy.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11QSizePolicyE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN11QSizePolicyE = QtMocHelpers::stringData(
    "QSizePolicy",
    "Policy",
    "Fixed",
    "Minimum",
    "Maximum",
    "Preferred",
    "MinimumExpanding",
    "Expanding",
    "Ignored",
    "ControlTypes",
    "ControlType",
    "DefaultType",
    "ButtonBox",
    "CheckBox",
    "ComboBox",
    "Frame",
    "GroupBox",
    "Label",
    "Line",
    "LineEdit",
    "PushButton",
    "RadioButton",
    "Slider",
    "SpinBox",
    "TabWidget",
    "ToolButton"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN11QSizePolicyE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       2,   14, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // enums: name, alias, flags, count, data
       1,    1, 0x0,    7,   24,
       9,   10, 0x1,   15,   38,

 // enum data: key, value
       2, uint(QSizePolicy::Fixed),
       3, uint(QSizePolicy::Minimum),
       4, uint(QSizePolicy::Maximum),
       5, uint(QSizePolicy::Preferred),
       6, uint(QSizePolicy::MinimumExpanding),
       7, uint(QSizePolicy::Expanding),
       8, uint(QSizePolicy::Ignored),
      11, uint(QSizePolicy::DefaultType),
      12, uint(QSizePolicy::ButtonBox),
      13, uint(QSizePolicy::CheckBox),
      14, uint(QSizePolicy::ComboBox),
      15, uint(QSizePolicy::Frame),
      16, uint(QSizePolicy::GroupBox),
      17, uint(QSizePolicy::Label),
      18, uint(QSizePolicy::Line),
      19, uint(QSizePolicy::LineEdit),
      20, uint(QSizePolicy::PushButton),
      21, uint(QSizePolicy::RadioButton),
      22, uint(QSizePolicy::Slider),
      23, uint(QSizePolicy::SpinBox),
      24, uint(QSizePolicy::TabWidget),
      25, uint(QSizePolicy::ToolButton),

       0        // eod
};

Q_CONSTINIT const QMetaObject QSizePolicy::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN11QSizePolicyE.offsetsAndSizes,
    qt_meta_data_ZN11QSizePolicyE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN11QSizePolicyE_t,
        // enum 'Policy'
        QtPrivate::TypeAndForceComplete<QSizePolicy::Policy, std::true_type>,
        // enum 'ControlTypes'
        QtPrivate::TypeAndForceComplete<QSizePolicy::ControlTypes, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QSizePolicy, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
