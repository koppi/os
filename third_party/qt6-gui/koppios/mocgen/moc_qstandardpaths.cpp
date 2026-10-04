/****************************************************************************
** Meta object code from reading C++ file 'qstandardpaths.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/corelib/io/qstandardpaths.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qstandardpaths.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14QStandardPathsE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14QStandardPathsE = QtMocHelpers::stringData(
    "QStandardPaths",
    "StandardLocation",
    "DesktopLocation",
    "DocumentsLocation",
    "FontsLocation",
    "ApplicationsLocation",
    "MusicLocation",
    "MoviesLocation",
    "PicturesLocation",
    "TempLocation",
    "HomeLocation",
    "AppLocalDataLocation",
    "CacheLocation",
    "GenericDataLocation",
    "RuntimeLocation",
    "ConfigLocation",
    "DownloadLocation",
    "GenericCacheLocation",
    "GenericConfigLocation",
    "AppDataLocation",
    "AppConfigLocation",
    "PublicShareLocation",
    "TemplatesLocation",
    "StateLocation",
    "GenericStateLocation",
    "LocateOptions",
    "LocateOption",
    "LocateFile",
    "LocateDirectory"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14QStandardPathsE[] = {

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
       1,    1, 0x0,   23,   24,
      25,   26, 0x1,    2,   70,

 // enum data: key, value
       2, uint(QStandardPaths::DesktopLocation),
       3, uint(QStandardPaths::DocumentsLocation),
       4, uint(QStandardPaths::FontsLocation),
       5, uint(QStandardPaths::ApplicationsLocation),
       6, uint(QStandardPaths::MusicLocation),
       7, uint(QStandardPaths::MoviesLocation),
       8, uint(QStandardPaths::PicturesLocation),
       9, uint(QStandardPaths::TempLocation),
      10, uint(QStandardPaths::HomeLocation),
      11, uint(QStandardPaths::AppLocalDataLocation),
      12, uint(QStandardPaths::CacheLocation),
      13, uint(QStandardPaths::GenericDataLocation),
      14, uint(QStandardPaths::RuntimeLocation),
      15, uint(QStandardPaths::ConfigLocation),
      16, uint(QStandardPaths::DownloadLocation),
      17, uint(QStandardPaths::GenericCacheLocation),
      18, uint(QStandardPaths::GenericConfigLocation),
      19, uint(QStandardPaths::AppDataLocation),
      20, uint(QStandardPaths::AppConfigLocation),
      21, uint(QStandardPaths::PublicShareLocation),
      22, uint(QStandardPaths::TemplatesLocation),
      23, uint(QStandardPaths::StateLocation),
      24, uint(QStandardPaths::GenericStateLocation),
      27, uint(QStandardPaths::LocateFile),
      28, uint(QStandardPaths::LocateDirectory),

       0        // eod
};

Q_CONSTINIT const QMetaObject QStandardPaths::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN14QStandardPathsE.offsetsAndSizes,
    qt_meta_data_ZN14QStandardPathsE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14QStandardPathsE_t,
        // enum 'StandardLocation'
        QtPrivate::TypeAndForceComplete<QStandardPaths::StandardLocation, std::true_type>,
        // enum 'LocateOptions'
        QtPrivate::TypeAndForceComplete<QStandardPaths::LocateOptions, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QStandardPaths, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
