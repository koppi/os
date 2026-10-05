/****************************************************************************
** Meta object code from reading C++ file 'qtoolbutton.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qtoolbutton.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qtoolbutton.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11QToolButtonE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN11QToolButtonE = QtMocHelpers::stringData(
    "QToolButton",
    "triggered",
    "",
    "QAction*",
    "showMenu",
    "setToolButtonStyle",
    "Qt::ToolButtonStyle",
    "style",
    "setDefaultAction",
    "popupMode",
    "ToolButtonPopupMode",
    "toolButtonStyle",
    "autoRaise",
    "arrowType",
    "Qt::ArrowType",
    "DelayedPopup",
    "MenuButtonPopup",
    "InstantPopup"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN11QToolButtonE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       4,   48, // properties
       1,   68, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   38,    2, 0x06,    6 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       4,    0,   41,    2, 0x0a,    8 /* Public */,
       5,    1,   42,    2, 0x0a,    9 /* Public */,
       8,    1,   45,    2, 0x0a,   11 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    2,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, 0x80000000 | 3,    2,

 // properties: name, type, flags, notifyId, revision
       9, 0x80000000 | 10, 0x0001510b, uint(-1), 0,
      11, 0x80000000 | 6, 0x0001510b, uint(-1), 0,
      12, QMetaType::Bool, 0x00015103, uint(-1), 0,
      13, 0x80000000 | 14, 0x0001510b, uint(-1), 0,

 // enums: name, alias, flags, count, data
      10,   10, 0x0,    3,   73,

 // enum data: key, value
      15, uint(QToolButton::DelayedPopup),
      16, uint(QToolButton::MenuButtonPopup),
      17, uint(QToolButton::InstantPopup),

       0        // eod
};

Q_CONSTINIT const QMetaObject QToolButton::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractButton::staticMetaObject>(),
    qt_meta_stringdata_ZN11QToolButtonE.offsetsAndSizes,
    qt_meta_data_ZN11QToolButtonE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN11QToolButtonE_t,
        // property 'popupMode'
        QtPrivate::TypeAndForceComplete<ToolButtonPopupMode, std::true_type>,
        // property 'toolButtonStyle'
        QtPrivate::TypeAndForceComplete<Qt::ToolButtonStyle, std::true_type>,
        // property 'autoRaise'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'arrowType'
        QtPrivate::TypeAndForceComplete<Qt::ArrowType, std::true_type>,
        // enum 'ToolButtonPopupMode'
        QtPrivate::TypeAndForceComplete<QToolButton::ToolButtonPopupMode, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QToolButton, std::true_type>,
        // method 'triggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QAction *, std::false_type>,
        // method 'showMenu'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setToolButtonStyle'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::ToolButtonStyle, std::false_type>,
        // method 'setDefaultAction'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QAction *, std::false_type>
    >,
    nullptr
} };

void QToolButton::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QToolButton *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->triggered((*reinterpret_cast< std::add_pointer_t<QAction*>>(_a[1]))); break;
        case 1: _t->showMenu(); break;
        case 2: _t->setToolButtonStyle((*reinterpret_cast< std::add_pointer_t<Qt::ToolButtonStyle>>(_a[1]))); break;
        case 3: _t->setDefaultAction((*reinterpret_cast< std::add_pointer_t<QAction*>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAction* >(); break;
            }
            break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAction* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QToolButton::*)(QAction * );
            if (_q_method_type _q_method = &QToolButton::triggered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< ToolButtonPopupMode*>(_v) = _t->popupMode(); break;
        case 1: *reinterpret_cast< Qt::ToolButtonStyle*>(_v) = _t->toolButtonStyle(); break;
        case 2: *reinterpret_cast< bool*>(_v) = _t->autoRaise(); break;
        case 3: *reinterpret_cast< Qt::ArrowType*>(_v) = _t->arrowType(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setPopupMode(*reinterpret_cast< ToolButtonPopupMode*>(_v)); break;
        case 1: _t->setToolButtonStyle(*reinterpret_cast< Qt::ToolButtonStyle*>(_v)); break;
        case 2: _t->setAutoRaise(*reinterpret_cast< bool*>(_v)); break;
        case 3: _t->setArrowType(*reinterpret_cast< Qt::ArrowType*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QToolButton::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QToolButton::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN11QToolButtonE.stringdata0))
        return static_cast<void*>(this);
    return QAbstractButton::qt_metacast(_clname);
}

int QToolButton::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractButton::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void QToolButton::triggered(QAction * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
