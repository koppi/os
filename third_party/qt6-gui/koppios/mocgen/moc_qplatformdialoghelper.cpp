/****************************************************************************
** Meta object code from reading C++ file 'qplatformdialoghelper.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/kernel/qplatformdialoghelper.h"
#include <QFont>
#include <QColor>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qplatformdialoghelper.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN21QPlatformDialogHelperE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN21QPlatformDialogHelperE = QtMocHelpers::stringData(
    "QPlatformDialogHelper",
    "accept",
    "",
    "reject",
    "StandardButtons",
    "StandardButton",
    "NoButton",
    "Ok",
    "Save",
    "SaveAll",
    "Open",
    "Yes",
    "YesToAll",
    "No",
    "NoToAll",
    "Abort",
    "Retry",
    "Ignore",
    "Close",
    "Cancel",
    "Discard",
    "Help",
    "Apply",
    "Reset",
    "RestoreDefaults",
    "FirstButton",
    "LastButton",
    "LowestBit",
    "HighestBit",
    "ButtonRole",
    "InvalidRole",
    "AcceptRole",
    "RejectRole",
    "DestructiveRole",
    "ActionRole",
    "HelpRole",
    "YesRole",
    "NoRole",
    "ResetRole",
    "ApplyRole",
    "NRoles",
    "RoleMask",
    "AlternateRole",
    "Stretch",
    "Reverse",
    "EOL",
    "ButtonLayout",
    "UnknownLayout",
    "WinLayout",
    "MacLayout",
    "KdeLayout",
    "GnomeLayout",
    "AndroidLayout"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN21QPlatformDialogHelperE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       3,   28, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   26,    2, 0x06,    4 /* Public */,
       3,    0,   27,    2, 0x06,    5 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,

 // enums: name, alias, flags, count, data
       4,    5, 0x1,   23,   43,
      29,   29, 0x0,   16,   89,
      46,   46, 0x0,    6,  121,

 // enum data: key, value
       6, uint(QPlatformDialogHelper::NoButton),
       7, uint(QPlatformDialogHelper::Ok),
       8, uint(QPlatformDialogHelper::Save),
       9, uint(QPlatformDialogHelper::SaveAll),
      10, uint(QPlatformDialogHelper::Open),
      11, uint(QPlatformDialogHelper::Yes),
      12, uint(QPlatformDialogHelper::YesToAll),
      13, uint(QPlatformDialogHelper::No),
      14, uint(QPlatformDialogHelper::NoToAll),
      15, uint(QPlatformDialogHelper::Abort),
      16, uint(QPlatformDialogHelper::Retry),
      17, uint(QPlatformDialogHelper::Ignore),
      18, uint(QPlatformDialogHelper::Close),
      19, uint(QPlatformDialogHelper::Cancel),
      20, uint(QPlatformDialogHelper::Discard),
      21, uint(QPlatformDialogHelper::Help),
      22, uint(QPlatformDialogHelper::Apply),
      23, uint(QPlatformDialogHelper::Reset),
      24, uint(QPlatformDialogHelper::RestoreDefaults),
      25, uint(QPlatformDialogHelper::FirstButton),
      26, uint(QPlatformDialogHelper::LastButton),
      27, uint(QPlatformDialogHelper::LowestBit),
      28, uint(QPlatformDialogHelper::HighestBit),
      30, uint(QPlatformDialogHelper::InvalidRole),
      31, uint(QPlatformDialogHelper::AcceptRole),
      32, uint(QPlatformDialogHelper::RejectRole),
      33, uint(QPlatformDialogHelper::DestructiveRole),
      34, uint(QPlatformDialogHelper::ActionRole),
      35, uint(QPlatformDialogHelper::HelpRole),
      36, uint(QPlatformDialogHelper::YesRole),
      37, uint(QPlatformDialogHelper::NoRole),
      38, uint(QPlatformDialogHelper::ResetRole),
      39, uint(QPlatformDialogHelper::ApplyRole),
      40, uint(QPlatformDialogHelper::NRoles),
      41, uint(QPlatformDialogHelper::RoleMask),
      42, uint(QPlatformDialogHelper::AlternateRole),
      43, uint(QPlatformDialogHelper::Stretch),
      44, uint(QPlatformDialogHelper::Reverse),
      45, uint(QPlatformDialogHelper::EOL),
      47, uint(QPlatformDialogHelper::UnknownLayout),
      48, uint(QPlatformDialogHelper::WinLayout),
      49, uint(QPlatformDialogHelper::MacLayout),
      50, uint(QPlatformDialogHelper::KdeLayout),
      51, uint(QPlatformDialogHelper::GnomeLayout),
      52, uint(QPlatformDialogHelper::AndroidLayout),

       0        // eod
};

Q_CONSTINIT const QMetaObject QPlatformDialogHelper::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN21QPlatformDialogHelperE.offsetsAndSizes,
    qt_meta_data_ZN21QPlatformDialogHelperE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN21QPlatformDialogHelperE_t,
        // enum 'StandardButtons'
        QtPrivate::TypeAndForceComplete<QPlatformDialogHelper::StandardButtons, std::true_type>,
        // enum 'ButtonRole'
        QtPrivate::TypeAndForceComplete<QPlatformDialogHelper::ButtonRole, std::true_type>,
        // enum 'ButtonLayout'
        QtPrivate::TypeAndForceComplete<QPlatformDialogHelper::ButtonLayout, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QPlatformDialogHelper, std::true_type>,
        // method 'accept'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'reject'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void QPlatformDialogHelper::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QPlatformDialogHelper *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->accept(); break;
        case 1: _t->reject(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QPlatformDialogHelper::*)();
            if (_q_method_type _q_method = &QPlatformDialogHelper::accept; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QPlatformDialogHelper::*)();
            if (_q_method_type _q_method = &QPlatformDialogHelper::reject; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *QPlatformDialogHelper::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QPlatformDialogHelper::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN21QPlatformDialogHelperE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int QPlatformDialogHelper::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void QPlatformDialogHelper::accept()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void QPlatformDialogHelper::reject()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
namespace {
struct qt_meta_tag_ZN19QColorDialogOptionsE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN19QColorDialogOptionsE = QtMocHelpers::stringData(
    "QColorDialogOptions",
    "ColorDialogOptions",
    "ColorDialogOption",
    "ShowAlphaChannel",
    "NoButtons",
    "DontUseNativeDialog",
    "NoEyeDropperButton"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN19QColorDialogOptionsE[] = {

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
       3, uint(QColorDialogOptions::ShowAlphaChannel),
       4, uint(QColorDialogOptions::NoButtons),
       5, uint(QColorDialogOptions::DontUseNativeDialog),
       6, uint(QColorDialogOptions::NoEyeDropperButton),

       0        // eod
};

Q_CONSTINIT const QMetaObject QColorDialogOptions::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN19QColorDialogOptionsE.offsetsAndSizes,
    qt_meta_data_ZN19QColorDialogOptionsE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN19QColorDialogOptionsE_t,
        // enum 'ColorDialogOptions'
        QtPrivate::TypeAndForceComplete<QColorDialogOptions::ColorDialogOptions, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QColorDialogOptions, std::true_type>
    >,
    nullptr
} };

namespace {
struct qt_meta_tag_ZN26QPlatformColorDialogHelperE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN26QPlatformColorDialogHelperE = QtMocHelpers::stringData(
    "QPlatformColorDialogHelper",
    "currentColorChanged",
    "",
    "color",
    "colorSelected"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN26QPlatformColorDialogHelperE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   26,    2, 0x06,    1 /* Public */,
       4,    1,   29,    2, 0x06,    3 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QColor,    3,
    QMetaType::Void, QMetaType::QColor,    3,

       0        // eod
};

Q_CONSTINIT const QMetaObject QPlatformColorDialogHelper::staticMetaObject = { {
    QMetaObject::SuperData::link<QPlatformDialogHelper::staticMetaObject>(),
    qt_meta_stringdata_ZN26QPlatformColorDialogHelperE.offsetsAndSizes,
    qt_meta_data_ZN26QPlatformColorDialogHelperE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN26QPlatformColorDialogHelperE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QPlatformColorDialogHelper, std::true_type>,
        // method 'currentColorChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QColor &, std::false_type>,
        // method 'colorSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QColor &, std::false_type>
    >,
    nullptr
} };

void QPlatformColorDialogHelper::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QPlatformColorDialogHelper *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->currentColorChanged((*reinterpret_cast< std::add_pointer_t<QColor>>(_a[1]))); break;
        case 1: _t->colorSelected((*reinterpret_cast< std::add_pointer_t<QColor>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QPlatformColorDialogHelper::*)(const QColor & );
            if (_q_method_type _q_method = &QPlatformColorDialogHelper::currentColorChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QPlatformColorDialogHelper::*)(const QColor & );
            if (_q_method_type _q_method = &QPlatformColorDialogHelper::colorSelected; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *QPlatformColorDialogHelper::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QPlatformColorDialogHelper::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN26QPlatformColorDialogHelperE.stringdata0))
        return static_cast<void*>(this);
    return QPlatformDialogHelper::qt_metacast(_clname);
}

int QPlatformColorDialogHelper::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QPlatformDialogHelper::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void QPlatformColorDialogHelper::currentColorChanged(const QColor & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QPlatformColorDialogHelper::colorSelected(const QColor & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
namespace {
struct qt_meta_tag_ZN18QFontDialogOptionsE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN18QFontDialogOptionsE = QtMocHelpers::stringData(
    "QFontDialogOptions",
    "FontDialogOptions",
    "FontDialogOption",
    "NoButtons",
    "DontUseNativeDialog",
    "ScalableFonts",
    "NonScalableFonts",
    "MonospacedFonts",
    "ProportionalFonts"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN18QFontDialogOptionsE[] = {

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
       1,    2, 0x1,    6,   19,

 // enum data: key, value
       3, uint(QFontDialogOptions::NoButtons),
       4, uint(QFontDialogOptions::DontUseNativeDialog),
       5, uint(QFontDialogOptions::ScalableFonts),
       6, uint(QFontDialogOptions::NonScalableFonts),
       7, uint(QFontDialogOptions::MonospacedFonts),
       8, uint(QFontDialogOptions::ProportionalFonts),

       0        // eod
};

Q_CONSTINIT const QMetaObject QFontDialogOptions::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN18QFontDialogOptionsE.offsetsAndSizes,
    qt_meta_data_ZN18QFontDialogOptionsE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN18QFontDialogOptionsE_t,
        // enum 'FontDialogOptions'
        QtPrivate::TypeAndForceComplete<QFontDialogOptions::FontDialogOptions, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QFontDialogOptions, std::true_type>
    >,
    nullptr
} };

namespace {
struct qt_meta_tag_ZN25QPlatformFontDialogHelperE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN25QPlatformFontDialogHelperE = QtMocHelpers::stringData(
    "QPlatformFontDialogHelper",
    "currentFontChanged",
    "",
    "font",
    "fontSelected"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN25QPlatformFontDialogHelperE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   26,    2, 0x06,    1 /* Public */,
       4,    1,   29,    2, 0x06,    3 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QFont,    3,
    QMetaType::Void, QMetaType::QFont,    3,

       0        // eod
};

Q_CONSTINIT const QMetaObject QPlatformFontDialogHelper::staticMetaObject = { {
    QMetaObject::SuperData::link<QPlatformDialogHelper::staticMetaObject>(),
    qt_meta_stringdata_ZN25QPlatformFontDialogHelperE.offsetsAndSizes,
    qt_meta_data_ZN25QPlatformFontDialogHelperE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN25QPlatformFontDialogHelperE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QPlatformFontDialogHelper, std::true_type>,
        // method 'currentFontChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QFont &, std::false_type>,
        // method 'fontSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QFont &, std::false_type>
    >,
    nullptr
} };

void QPlatformFontDialogHelper::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QPlatformFontDialogHelper *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->currentFontChanged((*reinterpret_cast< std::add_pointer_t<QFont>>(_a[1]))); break;
        case 1: _t->fontSelected((*reinterpret_cast< std::add_pointer_t<QFont>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QPlatformFontDialogHelper::*)(const QFont & );
            if (_q_method_type _q_method = &QPlatformFontDialogHelper::currentFontChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QPlatformFontDialogHelper::*)(const QFont & );
            if (_q_method_type _q_method = &QPlatformFontDialogHelper::fontSelected; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *QPlatformFontDialogHelper::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QPlatformFontDialogHelper::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN25QPlatformFontDialogHelperE.stringdata0))
        return static_cast<void*>(this);
    return QPlatformDialogHelper::qt_metacast(_clname);
}

int QPlatformFontDialogHelper::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QPlatformDialogHelper::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void QPlatformFontDialogHelper::currentFontChanged(const QFont & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QPlatformFontDialogHelper::fontSelected(const QFont & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
namespace {
struct qt_meta_tag_ZN18QFileDialogOptionsE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN18QFileDialogOptionsE = QtMocHelpers::stringData(
    "QFileDialogOptions",
    "ViewMode",
    "Detail",
    "List",
    "FileMode",
    "AnyFile",
    "ExistingFile",
    "Directory",
    "ExistingFiles",
    "DirectoryOnly",
    "AcceptMode",
    "AcceptOpen",
    "AcceptSave",
    "DialogLabel",
    "LookIn",
    "FileName",
    "FileType",
    "Accept",
    "Reject",
    "DialogLabelCount",
    "FileDialogOptions",
    "FileDialogOption",
    "ShowDirsOnly",
    "DontResolveSymlinks",
    "DontConfirmOverwrite",
    "DontUseNativeDialog",
    "ReadOnly",
    "HideNameFilterDetails",
    "DontUseCustomDirectoryIcons"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN18QFileDialogOptionsE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       5,   14, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // enums: name, alias, flags, count, data
       1,    1, 0x0,    2,   39,
       4,    4, 0x0,    5,   43,
      10,   10, 0x0,    2,   53,
      13,   13, 0x0,    6,   57,
      20,   21, 0x1,    7,   69,

 // enum data: key, value
       2, uint(QFileDialogOptions::Detail),
       3, uint(QFileDialogOptions::List),
       5, uint(QFileDialogOptions::AnyFile),
       6, uint(QFileDialogOptions::ExistingFile),
       7, uint(QFileDialogOptions::Directory),
       8, uint(QFileDialogOptions::ExistingFiles),
       9, uint(QFileDialogOptions::DirectoryOnly),
      11, uint(QFileDialogOptions::AcceptOpen),
      12, uint(QFileDialogOptions::AcceptSave),
      14, uint(QFileDialogOptions::LookIn),
      15, uint(QFileDialogOptions::FileName),
      16, uint(QFileDialogOptions::FileType),
      17, uint(QFileDialogOptions::Accept),
      18, uint(QFileDialogOptions::Reject),
      19, uint(QFileDialogOptions::DialogLabelCount),
      22, uint(QFileDialogOptions::ShowDirsOnly),
      23, uint(QFileDialogOptions::DontResolveSymlinks),
      24, uint(QFileDialogOptions::DontConfirmOverwrite),
      25, uint(QFileDialogOptions::DontUseNativeDialog),
      26, uint(QFileDialogOptions::ReadOnly),
      27, uint(QFileDialogOptions::HideNameFilterDetails),
      28, uint(QFileDialogOptions::DontUseCustomDirectoryIcons),

       0        // eod
};

Q_CONSTINIT const QMetaObject QFileDialogOptions::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN18QFileDialogOptionsE.offsetsAndSizes,
    qt_meta_data_ZN18QFileDialogOptionsE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN18QFileDialogOptionsE_t,
        // enum 'ViewMode'
        QtPrivate::TypeAndForceComplete<QFileDialogOptions::ViewMode, std::true_type>,
        // enum 'FileMode'
        QtPrivate::TypeAndForceComplete<QFileDialogOptions::FileMode, std::true_type>,
        // enum 'AcceptMode'
        QtPrivate::TypeAndForceComplete<QFileDialogOptions::AcceptMode, std::true_type>,
        // enum 'DialogLabel'
        QtPrivate::TypeAndForceComplete<QFileDialogOptions::DialogLabel, std::true_type>,
        // enum 'FileDialogOptions'
        QtPrivate::TypeAndForceComplete<QFileDialogOptions::FileDialogOptions, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QFileDialogOptions, std::true_type>
    >,
    nullptr
} };

namespace {
struct qt_meta_tag_ZN25QPlatformFileDialogHelperE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN25QPlatformFileDialogHelperE = QtMocHelpers::stringData(
    "QPlatformFileDialogHelper",
    "fileSelected",
    "",
    "file",
    "filesSelected",
    "QList<QUrl>",
    "files",
    "currentChanged",
    "path",
    "directoryEntered",
    "directory",
    "filterSelected",
    "filter"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN25QPlatformFileDialogHelperE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   44,    2, 0x06,    1 /* Public */,
       4,    1,   47,    2, 0x06,    3 /* Public */,
       7,    1,   50,    2, 0x06,    5 /* Public */,
       9,    1,   53,    2, 0x06,    7 /* Public */,
      11,    1,   56,    2, 0x06,    9 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QUrl,    3,
    QMetaType::Void, 0x80000000 | 5,    6,
    QMetaType::Void, QMetaType::QUrl,    8,
    QMetaType::Void, QMetaType::QUrl,   10,
    QMetaType::Void, QMetaType::QString,   12,

       0        // eod
};

Q_CONSTINIT const QMetaObject QPlatformFileDialogHelper::staticMetaObject = { {
    QMetaObject::SuperData::link<QPlatformDialogHelper::staticMetaObject>(),
    qt_meta_stringdata_ZN25QPlatformFileDialogHelperE.offsetsAndSizes,
    qt_meta_data_ZN25QPlatformFileDialogHelperE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN25QPlatformFileDialogHelperE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QPlatformFileDialogHelper, std::true_type>,
        // method 'fileSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QUrl &, std::false_type>,
        // method 'filesSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QList<QUrl> &, std::false_type>,
        // method 'currentChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QUrl &, std::false_type>,
        // method 'directoryEntered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QUrl &, std::false_type>,
        // method 'filterSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void QPlatformFileDialogHelper::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QPlatformFileDialogHelper *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->fileSelected((*reinterpret_cast< std::add_pointer_t<QUrl>>(_a[1]))); break;
        case 1: _t->filesSelected((*reinterpret_cast< std::add_pointer_t<QList<QUrl>>>(_a[1]))); break;
        case 2: _t->currentChanged((*reinterpret_cast< std::add_pointer_t<QUrl>>(_a[1]))); break;
        case 3: _t->directoryEntered((*reinterpret_cast< std::add_pointer_t<QUrl>>(_a[1]))); break;
        case 4: _t->filterSelected((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<QUrl> >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QPlatformFileDialogHelper::*)(const QUrl & );
            if (_q_method_type _q_method = &QPlatformFileDialogHelper::fileSelected; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QPlatformFileDialogHelper::*)(const QList<QUrl> & );
            if (_q_method_type _q_method = &QPlatformFileDialogHelper::filesSelected; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QPlatformFileDialogHelper::*)(const QUrl & );
            if (_q_method_type _q_method = &QPlatformFileDialogHelper::currentChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QPlatformFileDialogHelper::*)(const QUrl & );
            if (_q_method_type _q_method = &QPlatformFileDialogHelper::directoryEntered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QPlatformFileDialogHelper::*)(const QString & );
            if (_q_method_type _q_method = &QPlatformFileDialogHelper::filterSelected; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
    }
}

const QMetaObject *QPlatformFileDialogHelper::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QPlatformFileDialogHelper::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN25QPlatformFileDialogHelperE.stringdata0))
        return static_cast<void*>(this);
    return QPlatformDialogHelper::qt_metacast(_clname);
}

int QPlatformFileDialogHelper::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QPlatformDialogHelper::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void QPlatformFileDialogHelper::fileSelected(const QUrl & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QPlatformFileDialogHelper::filesSelected(const QList<QUrl> & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QPlatformFileDialogHelper::currentChanged(const QUrl & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QPlatformFileDialogHelper::directoryEntered(const QUrl & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void QPlatformFileDialogHelper::filterSelected(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}
namespace {
struct qt_meta_tag_ZN21QMessageDialogOptionsE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN21QMessageDialogOptionsE = QtMocHelpers::stringData(
    "QMessageDialogOptions",
    "Options",
    "Option",
    "DontUseNativeDialog",
    "StandardIcon",
    "NoIcon",
    "Information",
    "Warning",
    "Critical",
    "Question"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN21QMessageDialogOptionsE[] = {

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
       1,    2, 0x3,    1,   24,
       4,    4, 0x0,    5,   26,

 // enum data: key, value
       3, uint(QMessageDialogOptions::Option::DontUseNativeDialog),
       5, uint(QMessageDialogOptions::NoIcon),
       6, uint(QMessageDialogOptions::Information),
       7, uint(QMessageDialogOptions::Warning),
       8, uint(QMessageDialogOptions::Critical),
       9, uint(QMessageDialogOptions::Question),

       0        // eod
};

Q_CONSTINIT const QMetaObject QMessageDialogOptions::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN21QMessageDialogOptionsE.offsetsAndSizes,
    qt_meta_data_ZN21QMessageDialogOptionsE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN21QMessageDialogOptionsE_t,
        // enum 'Options'
        QtPrivate::TypeAndForceComplete<QMessageDialogOptions::Options, std::true_type>,
        // enum 'StandardIcon'
        QtPrivate::TypeAndForceComplete<QMessageDialogOptions::StandardIcon, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QMessageDialogOptions, std::true_type>
    >,
    nullptr
} };

namespace {
struct qt_meta_tag_ZN28QPlatformMessageDialogHelperE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN28QPlatformMessageDialogHelperE = QtMocHelpers::stringData(
    "QPlatformMessageDialogHelper",
    "clicked",
    "",
    "QPlatformDialogHelper::StandardButton",
    "button",
    "QPlatformDialogHelper::ButtonRole",
    "role",
    "checkBoxStateChanged",
    "Qt::CheckState",
    "state"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN28QPlatformMessageDialogHelperE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   26,    2, 0x06,    1 /* Public */,
       7,    1,   31,    2, 0x06,    4 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3, 0x80000000 | 5,    4,    6,
    QMetaType::Void, 0x80000000 | 8,    9,

       0        // eod
};

Q_CONSTINIT const QMetaObject QPlatformMessageDialogHelper::staticMetaObject = { {
    QMetaObject::SuperData::link<QPlatformDialogHelper::staticMetaObject>(),
    qt_meta_stringdata_ZN28QPlatformMessageDialogHelperE.offsetsAndSizes,
    qt_meta_data_ZN28QPlatformMessageDialogHelperE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN28QPlatformMessageDialogHelperE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QPlatformMessageDialogHelper, std::true_type>,
        // method 'clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QPlatformDialogHelper::StandardButton, std::false_type>,
        QtPrivate::TypeAndForceComplete<QPlatformDialogHelper::ButtonRole, std::false_type>,
        // method 'checkBoxStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::CheckState, std::false_type>
    >,
    nullptr
} };

void QPlatformMessageDialogHelper::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QPlatformMessageDialogHelper *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->clicked((*reinterpret_cast< std::add_pointer_t<QPlatformDialogHelper::StandardButton>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QPlatformDialogHelper::ButtonRole>>(_a[2]))); break;
        case 1: _t->checkBoxStateChanged((*reinterpret_cast< std::add_pointer_t<Qt::CheckState>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QPlatformDialogHelper::ButtonRole >(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QPlatformDialogHelper::StandardButton >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QPlatformMessageDialogHelper::*)(QPlatformDialogHelper::StandardButton , QPlatformDialogHelper::ButtonRole );
            if (_q_method_type _q_method = &QPlatformMessageDialogHelper::clicked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QPlatformMessageDialogHelper::*)(Qt::CheckState );
            if (_q_method_type _q_method = &QPlatformMessageDialogHelper::checkBoxStateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *QPlatformMessageDialogHelper::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QPlatformMessageDialogHelper::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN28QPlatformMessageDialogHelperE.stringdata0))
        return static_cast<void*>(this);
    return QPlatformDialogHelper::qt_metacast(_clname);
}

int QPlatformMessageDialogHelper::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QPlatformDialogHelper::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void QPlatformMessageDialogHelper::clicked(QPlatformDialogHelper::StandardButton _t1, QPlatformDialogHelper::ButtonRole _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QPlatformMessageDialogHelper::checkBoxStateChanged(Qt::CheckState _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
