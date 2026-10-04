/****************************************************************************
** Meta object code from reading C++ file 'qpalette.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/kernel/qpalette.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qpalette.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN8QPaletteE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN8QPaletteE = QtMocHelpers::stringData(
    "QPalette",
    "ColorGroup",
    "Active",
    "Disabled",
    "Inactive",
    "NColorGroups",
    "Current",
    "All",
    "Normal",
    "ColorRole",
    "WindowText",
    "Button",
    "Light",
    "Midlight",
    "Dark",
    "Mid",
    "Text",
    "BrightText",
    "ButtonText",
    "Base",
    "Window",
    "Shadow",
    "Highlight",
    "HighlightedText",
    "Link",
    "LinkVisited",
    "AlternateBase",
    "NoRole",
    "ToolTipBase",
    "ToolTipText",
    "PlaceholderText",
    "Accent",
    "NColorRoles"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN8QPaletteE[] = {

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
       9,    9, 0x0,   23,   38,

 // enum data: key, value
       2, uint(QPalette::Active),
       3, uint(QPalette::Disabled),
       4, uint(QPalette::Inactive),
       5, uint(QPalette::NColorGroups),
       6, uint(QPalette::Current),
       7, uint(QPalette::All),
       8, uint(QPalette::Normal),
      10, uint(QPalette::WindowText),
      11, uint(QPalette::Button),
      12, uint(QPalette::Light),
      13, uint(QPalette::Midlight),
      14, uint(QPalette::Dark),
      15, uint(QPalette::Mid),
      16, uint(QPalette::Text),
      17, uint(QPalette::BrightText),
      18, uint(QPalette::ButtonText),
      19, uint(QPalette::Base),
      20, uint(QPalette::Window),
      21, uint(QPalette::Shadow),
      22, uint(QPalette::Highlight),
      23, uint(QPalette::HighlightedText),
      24, uint(QPalette::Link),
      25, uint(QPalette::LinkVisited),
      26, uint(QPalette::AlternateBase),
      27, uint(QPalette::NoRole),
      28, uint(QPalette::ToolTipBase),
      29, uint(QPalette::ToolTipText),
      30, uint(QPalette::PlaceholderText),
      31, uint(QPalette::Accent),
      32, uint(QPalette::NColorRoles),

       0        // eod
};

Q_CONSTINIT const QMetaObject QPalette::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN8QPaletteE.offsetsAndSizes,
    qt_meta_data_ZN8QPaletteE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN8QPaletteE_t,
        // enum 'ColorGroup'
        QtPrivate::TypeAndForceComplete<QPalette::ColorGroup, std::true_type>,
        // enum 'ColorRole'
        QtPrivate::TypeAndForceComplete<QPalette::ColorRole, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QPalette, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
