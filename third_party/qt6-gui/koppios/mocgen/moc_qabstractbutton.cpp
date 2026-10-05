/****************************************************************************
** Meta object code from reading C++ file 'qabstractbutton.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qabstractbutton.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qabstractbutton.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15QAbstractButtonE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN15QAbstractButtonE = QtMocHelpers::stringData(
    "QAbstractButton",
    "pressed",
    "",
    "released",
    "clicked",
    "checked",
    "toggled",
    "setIconSize",
    "size",
    "animateClick",
    "click",
    "toggle",
    "setChecked",
    "text",
    "icon",
    "iconSize",
    "shortcut",
    "QKeySequence",
    "checkable",
    "autoRepeat",
    "autoExclusive",
    "autoRepeatDelay",
    "autoRepeatInterval",
    "down"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN15QAbstractButtonE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
      11,   92, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   74,    2, 0x06,   12 /* Public */,
       3,    0,   75,    2, 0x06,   13 /* Public */,
       4,    1,   76,    2, 0x06,   14 /* Public */,
       4,    0,   79,    2, 0x26,   16 /* Public | MethodCloned */,
       6,    1,   80,    2, 0x06,   17 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       7,    1,   83,    2, 0x0a,   19 /* Public */,
       9,    0,   86,    2, 0x0a,   21 /* Public */,
      10,    0,   87,    2, 0x0a,   22 /* Public */,
      11,    0,   88,    2, 0x0a,   23 /* Public */,
      12,    1,   89,    2, 0x0a,   24 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    5,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    5,

 // slots: parameters
    QMetaType::Void, QMetaType::QSize,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    2,

 // properties: name, type, flags, notifyId, revision
      13, QMetaType::QString, 0x00015103, uint(-1), 0,
      14, QMetaType::QIcon, 0x00015103, uint(-1), 0,
      15, QMetaType::QSize, 0x00015103, uint(-1), 0,
      16, 0x80000000 | 17, 0x0001510b, uint(-1), 0,
      18, QMetaType::Bool, 0x00015103, uint(-1), 0,
       5, QMetaType::Bool, 0x00115103, uint(4), 0,
      19, QMetaType::Bool, 0x00015103, uint(-1), 0,
      20, QMetaType::Bool, 0x00015103, uint(-1), 0,
      21, QMetaType::Int, 0x00015103, uint(-1), 0,
      22, QMetaType::Int, 0x00015103, uint(-1), 0,
      23, QMetaType::Bool, 0x00014103, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QAbstractButton::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN15QAbstractButtonE.offsetsAndSizes,
    qt_meta_data_ZN15QAbstractButtonE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN15QAbstractButtonE_t,
        // property 'text'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'icon'
        QtPrivate::TypeAndForceComplete<QIcon, std::true_type>,
        // property 'iconSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'shortcut'
        QtPrivate::TypeAndForceComplete<QKeySequence, std::true_type>,
        // property 'checkable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'checked'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'autoRepeat'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'autoExclusive'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'autoRepeatDelay'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'autoRepeatInterval'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'down'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QAbstractButton, std::true_type>,
        // method 'pressed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'released'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setIconSize'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QSize &, std::false_type>,
        // method 'animateClick'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'click'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggle'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setChecked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void QAbstractButton::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QAbstractButton *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->pressed(); break;
        case 1: _t->released(); break;
        case 2: _t->clicked((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->clicked(); break;
        case 4: _t->toggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->setIconSize((*reinterpret_cast< std::add_pointer_t<QSize>>(_a[1]))); break;
        case 6: _t->animateClick(); break;
        case 7: _t->click(); break;
        case 8: _t->toggle(); break;
        case 9: _t->setChecked((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QAbstractButton::*)();
            if (_q_method_type _q_method = &QAbstractButton::pressed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QAbstractButton::*)();
            if (_q_method_type _q_method = &QAbstractButton::released; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QAbstractButton::*)(bool );
            if (_q_method_type _q_method = &QAbstractButton::clicked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QAbstractButton::*)(bool );
            if (_q_method_type _q_method = &QAbstractButton::toggled; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->text(); break;
        case 1: *reinterpret_cast< QIcon*>(_v) = _t->icon(); break;
        case 2: *reinterpret_cast< QSize*>(_v) = _t->iconSize(); break;
        case 3: *reinterpret_cast< QKeySequence*>(_v) = _t->shortcut(); break;
        case 4: *reinterpret_cast< bool*>(_v) = _t->isCheckable(); break;
        case 5: *reinterpret_cast< bool*>(_v) = _t->isChecked(); break;
        case 6: *reinterpret_cast< bool*>(_v) = _t->autoRepeat(); break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->autoExclusive(); break;
        case 8: *reinterpret_cast< int*>(_v) = _t->autoRepeatDelay(); break;
        case 9: *reinterpret_cast< int*>(_v) = _t->autoRepeatInterval(); break;
        case 10: *reinterpret_cast< bool*>(_v) = _t->isDown(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setText(*reinterpret_cast< QString*>(_v)); break;
        case 1: _t->setIcon(*reinterpret_cast< QIcon*>(_v)); break;
        case 2: _t->setIconSize(*reinterpret_cast< QSize*>(_v)); break;
        case 3: _t->setShortcut(*reinterpret_cast< QKeySequence*>(_v)); break;
        case 4: _t->setCheckable(*reinterpret_cast< bool*>(_v)); break;
        case 5: _t->setChecked(*reinterpret_cast< bool*>(_v)); break;
        case 6: _t->setAutoRepeat(*reinterpret_cast< bool*>(_v)); break;
        case 7: _t->setAutoExclusive(*reinterpret_cast< bool*>(_v)); break;
        case 8: _t->setAutoRepeatDelay(*reinterpret_cast< int*>(_v)); break;
        case 9: _t->setAutoRepeatInterval(*reinterpret_cast< int*>(_v)); break;
        case 10: _t->setDown(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QAbstractButton::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QAbstractButton::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN15QAbstractButtonE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QAbstractButton::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void QAbstractButton::pressed()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void QAbstractButton::released()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void QAbstractButton::clicked(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 4
void QAbstractButton::toggled(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}
QT_WARNING_POP
