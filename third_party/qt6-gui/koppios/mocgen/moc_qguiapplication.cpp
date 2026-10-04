/****************************************************************************
** Meta object code from reading C++ file 'qguiapplication.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/kernel/qguiapplication.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qguiapplication.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15QGuiApplicationE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN15QGuiApplicationE = QtMocHelpers::stringData(
    "QGuiApplication",
    "fontDatabaseChanged",
    "",
    "screenAdded",
    "QScreen*",
    "screen",
    "screenRemoved",
    "primaryScreenChanged",
    "lastWindowClosed",
    "focusObjectChanged",
    "focusObject",
    "focusWindowChanged",
    "QWindow*",
    "focusWindow",
    "applicationStateChanged",
    "Qt::ApplicationState",
    "state",
    "layoutDirectionChanged",
    "Qt::LayoutDirection",
    "direction",
    "applicationDisplayNameChanged",
    "setBadgeNumber",
    "number",
    "_q_updateFocusObject",
    "object",
    "windowIcon",
    "applicationDisplayName",
    "desktopFileName",
    "layoutDirection",
    "platformName",
    "quitOnLastWindowClosed",
    "primaryScreen"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN15QGuiApplicationE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       7,  116, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      10,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   86,    2, 0x06,    8 /* Public */,
       3,    1,   87,    2, 0x06,    9 /* Public */,
       6,    1,   90,    2, 0x06,   11 /* Public */,
       7,    1,   93,    2, 0x06,   13 /* Public */,
       8,    0,   96,    2, 0x06,   15 /* Public */,
       9,    1,   97,    2, 0x06,   16 /* Public */,
      11,    1,  100,    2, 0x06,   18 /* Public */,
      14,    1,  103,    2, 0x06,   20 /* Public */,
      17,    1,  106,    2, 0x06,   22 /* Public */,
      20,    0,  109,    2, 0x06,   24 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      21,    1,  110,    2, 0x0a,   25 /* Public */,
      23,    1,  113,    2, 0x08,   27 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 4,    5,
    QMetaType::Void, 0x80000000 | 4,    5,
    QMetaType::Void, 0x80000000 | 4,    5,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QObjectStar,   10,
    QMetaType::Void, 0x80000000 | 12,   13,
    QMetaType::Void, 0x80000000 | 15,   16,
    QMetaType::Void, 0x80000000 | 18,   19,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::LongLong,   22,
    QMetaType::Void, QMetaType::QObjectStar,   24,

 // properties: name, type, flags, notifyId, revision
      25, QMetaType::QIcon, 0x00015103, uint(-1), 0,
      26, QMetaType::QString, 0x00015103, uint(9), 0,
      27, QMetaType::QString, 0x00015103, uint(-1), 0,
      28, 0x80000000 | 18, 0x0001510b, uint(8), 0,
      29, QMetaType::QString, 0x00005401, uint(-1), 0,
      30, QMetaType::Bool, 0x00015103, uint(-1), 0,
      31, 0x80000000 | 4, 0x00005009, uint(3), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QGuiApplication::staticMetaObject = { {
    QMetaObject::SuperData::link<QCoreApplication::staticMetaObject>(),
    qt_meta_stringdata_ZN15QGuiApplicationE.offsetsAndSizes,
    qt_meta_data_ZN15QGuiApplicationE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN15QGuiApplicationE_t,
        // property 'windowIcon'
        QtPrivate::TypeAndForceComplete<QIcon, std::true_type>,
        // property 'applicationDisplayName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'desktopFileName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'layoutDirection'
        QtPrivate::TypeAndForceComplete<Qt::LayoutDirection, std::true_type>,
        // property 'platformName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'quitOnLastWindowClosed'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'primaryScreen'
        QtPrivate::TypeAndForceComplete<QScreen*, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QGuiApplication, std::true_type>,
        // method 'fontDatabaseChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'screenAdded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QScreen *, std::false_type>,
        // method 'screenRemoved'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QScreen *, std::false_type>,
        // method 'primaryScreenChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QScreen *, std::false_type>,
        // method 'lastWindowClosed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'focusObjectChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QObject *, std::false_type>,
        // method 'focusWindowChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QWindow *, std::false_type>,
        // method 'applicationStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::ApplicationState, std::false_type>,
        // method 'layoutDirectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::LayoutDirection, std::false_type>,
        // method 'applicationDisplayNameChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setBadgeNumber'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method '_q_updateFocusObject'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QObject *, std::false_type>
    >,
    nullptr
} };

void QGuiApplication::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QGuiApplication *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->fontDatabaseChanged(); break;
        case 1: _t->screenAdded((*reinterpret_cast< std::add_pointer_t<QScreen*>>(_a[1]))); break;
        case 2: _t->screenRemoved((*reinterpret_cast< std::add_pointer_t<QScreen*>>(_a[1]))); break;
        case 3: _t->primaryScreenChanged((*reinterpret_cast< std::add_pointer_t<QScreen*>>(_a[1]))); break;
        case 4: _t->lastWindowClosed(); break;
        case 5: _t->focusObjectChanged((*reinterpret_cast< std::add_pointer_t<QObject*>>(_a[1]))); break;
        case 6: _t->focusWindowChanged((*reinterpret_cast< std::add_pointer_t<QWindow*>>(_a[1]))); break;
        case 7: _t->applicationStateChanged((*reinterpret_cast< std::add_pointer_t<Qt::ApplicationState>>(_a[1]))); break;
        case 8: _t->layoutDirectionChanged((*reinterpret_cast< std::add_pointer_t<Qt::LayoutDirection>>(_a[1]))); break;
        case 9: _t->applicationDisplayNameChanged(); break;
        case 10: _t->setBadgeNumber((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 11: _t->d_func()->_q_updateFocusObject((*reinterpret_cast< std::add_pointer_t<QObject*>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QGuiApplication::*)();
            if (_q_method_type _q_method = &QGuiApplication::fontDatabaseChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QGuiApplication::*)(QScreen * );
            if (_q_method_type _q_method = &QGuiApplication::screenAdded; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QGuiApplication::*)(QScreen * );
            if (_q_method_type _q_method = &QGuiApplication::screenRemoved; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QGuiApplication::*)(QScreen * );
            if (_q_method_type _q_method = &QGuiApplication::primaryScreenChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QGuiApplication::*)();
            if (_q_method_type _q_method = &QGuiApplication::lastWindowClosed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (QGuiApplication::*)(QObject * );
            if (_q_method_type _q_method = &QGuiApplication::focusObjectChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (QGuiApplication::*)(QWindow * );
            if (_q_method_type _q_method = &QGuiApplication::focusWindowChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (QGuiApplication::*)(Qt::ApplicationState );
            if (_q_method_type _q_method = &QGuiApplication::applicationStateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (QGuiApplication::*)(Qt::LayoutDirection );
            if (_q_method_type _q_method = &QGuiApplication::layoutDirectionChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (QGuiApplication::*)();
            if (_q_method_type _q_method = &QGuiApplication::applicationDisplayNameChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QIcon*>(_v) = _t->windowIcon(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->applicationDisplayName(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->desktopFileName(); break;
        case 3: *reinterpret_cast< Qt::LayoutDirection*>(_v) = _t->layoutDirection(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->platformName(); break;
        case 5: *reinterpret_cast< bool*>(_v) = _t->quitOnLastWindowClosed(); break;
        case 6: *reinterpret_cast< QScreen**>(_v) = _t->primaryScreen(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setWindowIcon(*reinterpret_cast< QIcon*>(_v)); break;
        case 1: _t->setApplicationDisplayName(*reinterpret_cast< QString*>(_v)); break;
        case 2: _t->setDesktopFileName(*reinterpret_cast< QString*>(_v)); break;
        case 3: _t->setLayoutDirection(*reinterpret_cast< Qt::LayoutDirection*>(_v)); break;
        case 5: _t->setQuitOnLastWindowClosed(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QGuiApplication::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QGuiApplication::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN15QGuiApplicationE.stringdata0))
        return static_cast<void*>(this);
    return QCoreApplication::qt_metacast(_clname);
}

int QGuiApplication::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QCoreApplication::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void QGuiApplication::fontDatabaseChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void QGuiApplication::screenAdded(QScreen * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QGuiApplication::screenRemoved(QScreen * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QGuiApplication::primaryScreenChanged(QScreen * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void QGuiApplication::lastWindowClosed()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void QGuiApplication::focusObjectChanged(QObject * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void QGuiApplication::focusWindowChanged(QWindow * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void QGuiApplication::applicationStateChanged(Qt::ApplicationState _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void QGuiApplication::layoutDirectionChanged(Qt::LayoutDirection _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void QGuiApplication::applicationDisplayNameChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}
QT_WARNING_POP
