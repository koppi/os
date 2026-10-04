/****************************************************************************
** Meta object code from reading C++ file 'qaction.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/kernel/qaction.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qaction.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7QActionE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN7QActionE = QtMocHelpers::stringData(
    "QAction",
    "changed",
    "",
    "enabledChanged",
    "enabled",
    "checkableChanged",
    "checkable",
    "visibleChanged",
    "triggered",
    "checked",
    "hovered",
    "toggled",
    "trigger",
    "hover",
    "setChecked",
    "toggle",
    "setEnabled",
    "resetEnabled",
    "setDisabled",
    "b",
    "setVisible",
    "icon",
    "text",
    "iconText",
    "toolTip",
    "statusTip",
    "whatsThis",
    "font",
    "shortcut",
    "QKeySequence",
    "shortcutContext",
    "Qt::ShortcutContext",
    "autoRepeat",
    "visible",
    "menuRole",
    "MenuRole",
    "iconVisibleInMenu",
    "shortcutVisibleInContextMenu",
    "priority",
    "Priority",
    "NoRole",
    "TextHeuristicRole",
    "ApplicationSpecificRole",
    "AboutQtRole",
    "AboutRole",
    "PreferencesRole",
    "QuitRole",
    "LowPriority",
    "NormalPriority",
    "HighPriority"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN7QActionE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      16,   14, // methods
      18,  142, // properties
       2,  232, // enums/sets
       0,    0, // constructors
       0,       // flags
       8,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  110,    2, 0x06,   21 /* Public */,
       3,    1,  111,    2, 0x06,   22 /* Public */,
       5,    1,  114,    2, 0x06,   24 /* Public */,
       7,    0,  117,    2, 0x06,   26 /* Public */,
       8,    1,  118,    2, 0x06,   27 /* Public */,
       8,    0,  121,    2, 0x26,   29 /* Public | MethodCloned */,
      10,    0,  122,    2, 0x06,   30 /* Public */,
      11,    1,  123,    2, 0x06,   31 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      12,    0,  126,    2, 0x0a,   33 /* Public */,
      13,    0,  127,    2, 0x0a,   34 /* Public */,
      14,    1,  128,    2, 0x0a,   35 /* Public */,
      15,    0,  131,    2, 0x0a,   37 /* Public */,
      16,    1,  132,    2, 0x0a,   38 /* Public */,
      17,    0,  135,    2, 0x0a,   40 /* Public */,
      18,    1,  136,    2, 0x0a,   41 /* Public */,
      20,    1,  139,    2, 0x0a,   43 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    4,
    QMetaType::Void, QMetaType::Bool,    6,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    9,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    2,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    2,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    2,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   19,
    QMetaType::Void, QMetaType::Bool,    2,

 // properties: name, type, flags, notifyId, revision
       6, QMetaType::Bool, 0x00015903, uint(2), 0,
       9, QMetaType::Bool, 0x00015103, uint(7), 0,
       4, QMetaType::Bool, 0x00015907, uint(1), 0,
      21, QMetaType::QIcon, 0x00015103, uint(0), 0,
      22, QMetaType::QString, 0x00015103, uint(0), 0,
      23, QMetaType::QString, 0x00015103, uint(0), 0,
      24, QMetaType::QString, 0x00015103, uint(0), 0,
      25, QMetaType::QString, 0x00015103, uint(0), 0,
      26, QMetaType::QString, 0x00015103, uint(0), 0,
      27, QMetaType::QFont, 0x00015103, uint(0), 0,
      28, 0x80000000 | 29, 0x0001510b, uint(0), 0,
      30, 0x80000000 | 31, 0x0001510b, uint(0), 0,
      32, QMetaType::Bool, 0x00015103, uint(0), 0,
      33, QMetaType::Bool, 0x00015903, uint(3), 0,
      34, 0x80000000 | 35, 0x0001510b, uint(0), 0,
      36, QMetaType::Bool, 0x00015103, uint(0), 0,
      37, QMetaType::Bool, 0x00015103, uint(0), 0,
      38, 0x80000000 | 39, 0x0001510b, uint(0), 0,

 // enums: name, alias, flags, count, data
      35,   35, 0x0,    7,  242,
      39,   39, 0x0,    3,  256,

 // enum data: key, value
      40, uint(QAction::NoRole),
      41, uint(QAction::TextHeuristicRole),
      42, uint(QAction::ApplicationSpecificRole),
      43, uint(QAction::AboutQtRole),
      44, uint(QAction::AboutRole),
      45, uint(QAction::PreferencesRole),
      46, uint(QAction::QuitRole),
      47, uint(QAction::LowPriority),
      48, uint(QAction::NormalPriority),
      49, uint(QAction::HighPriority),

       0        // eod
};

Q_CONSTINIT const QMetaObject QAction::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN7QActionE.offsetsAndSizes,
    qt_meta_data_ZN7QActionE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN7QActionE_t,
        // property 'checkable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'checked'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'enabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'icon'
        QtPrivate::TypeAndForceComplete<QIcon, std::true_type>,
        // property 'text'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'iconText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'toolTip'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'statusTip'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'whatsThis'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'font'
        QtPrivate::TypeAndForceComplete<QFont, std::true_type>,
        // property 'shortcut'
        QtPrivate::TypeAndForceComplete<QKeySequence, std::true_type>,
        // property 'shortcutContext'
        QtPrivate::TypeAndForceComplete<Qt::ShortcutContext, std::true_type>,
        // property 'autoRepeat'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'visible'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'menuRole'
        QtPrivate::TypeAndForceComplete<MenuRole, std::true_type>,
        // property 'iconVisibleInMenu'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'shortcutVisibleInContextMenu'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'priority'
        QtPrivate::TypeAndForceComplete<Priority, std::true_type>,
        // enum 'MenuRole'
        QtPrivate::TypeAndForceComplete<QAction::MenuRole, std::true_type>,
        // enum 'Priority'
        QtPrivate::TypeAndForceComplete<QAction::Priority, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QAction, std::true_type>,
        // method 'changed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'enabledChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'checkableChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'visibleChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'triggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'triggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'hovered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'trigger'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'hover'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setChecked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'toggle'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setEnabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'resetEnabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setDisabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setVisible'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void QAction::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QAction *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->changed(); break;
        case 1: _t->enabledChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 2: _t->checkableChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->visibleChanged(); break;
        case 4: _t->triggered((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->triggered(); break;
        case 6: _t->hovered(); break;
        case 7: _t->toggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 8: _t->trigger(); break;
        case 9: _t->hover(); break;
        case 10: _t->setChecked((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 11: _t->toggle(); break;
        case 12: _t->setEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 13: _t->resetEnabled(); break;
        case 14: _t->setDisabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 15: _t->setVisible((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QAction::*)();
            if (_q_method_type _q_method = &QAction::changed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QAction::*)(bool );
            if (_q_method_type _q_method = &QAction::enabledChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QAction::*)(bool );
            if (_q_method_type _q_method = &QAction::checkableChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QAction::*)();
            if (_q_method_type _q_method = &QAction::visibleChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QAction::*)(bool );
            if (_q_method_type _q_method = &QAction::triggered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (QAction::*)();
            if (_q_method_type _q_method = &QAction::hovered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (QAction::*)(bool );
            if (_q_method_type _q_method = &QAction::toggled; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isCheckable(); break;
        case 1: *reinterpret_cast< bool*>(_v) = _t->isChecked(); break;
        case 2: *reinterpret_cast< bool*>(_v) = _t->isEnabled(); break;
        case 3: *reinterpret_cast< QIcon*>(_v) = _t->icon(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->text(); break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->iconText(); break;
        case 6: *reinterpret_cast< QString*>(_v) = _t->toolTip(); break;
        case 7: *reinterpret_cast< QString*>(_v) = _t->statusTip(); break;
        case 8: *reinterpret_cast< QString*>(_v) = _t->whatsThis(); break;
        case 9: *reinterpret_cast< QFont*>(_v) = _t->font(); break;
        case 10: *reinterpret_cast< QKeySequence*>(_v) = _t->shortcut(); break;
        case 11: *reinterpret_cast< Qt::ShortcutContext*>(_v) = _t->shortcutContext(); break;
        case 12: *reinterpret_cast< bool*>(_v) = _t->autoRepeat(); break;
        case 13: *reinterpret_cast< bool*>(_v) = _t->isVisible(); break;
        case 14: *reinterpret_cast< MenuRole*>(_v) = _t->menuRole(); break;
        case 15: *reinterpret_cast< bool*>(_v) = _t->isIconVisibleInMenu(); break;
        case 16: *reinterpret_cast< bool*>(_v) = _t->isShortcutVisibleInContextMenu(); break;
        case 17: *reinterpret_cast< Priority*>(_v) = _t->priority(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setCheckable(*reinterpret_cast< bool*>(_v)); break;
        case 1: _t->setChecked(*reinterpret_cast< bool*>(_v)); break;
        case 2: _t->setEnabled(*reinterpret_cast< bool*>(_v)); break;
        case 3: _t->setIcon(*reinterpret_cast< QIcon*>(_v)); break;
        case 4: _t->setText(*reinterpret_cast< QString*>(_v)); break;
        case 5: _t->setIconText(*reinterpret_cast< QString*>(_v)); break;
        case 6: _t->setToolTip(*reinterpret_cast< QString*>(_v)); break;
        case 7: _t->setStatusTip(*reinterpret_cast< QString*>(_v)); break;
        case 8: _t->setWhatsThis(*reinterpret_cast< QString*>(_v)); break;
        case 9: _t->setFont(*reinterpret_cast< QFont*>(_v)); break;
        case 10: _t->setShortcut(*reinterpret_cast< QKeySequence*>(_v)); break;
        case 11: _t->setShortcutContext(*reinterpret_cast< Qt::ShortcutContext*>(_v)); break;
        case 12: _t->setAutoRepeat(*reinterpret_cast< bool*>(_v)); break;
        case 13: _t->setVisible(*reinterpret_cast< bool*>(_v)); break;
        case 14: _t->setMenuRole(*reinterpret_cast< MenuRole*>(_v)); break;
        case 15: _t->setIconVisibleInMenu(*reinterpret_cast< bool*>(_v)); break;
        case 16: _t->setShortcutVisibleInContextMenu(*reinterpret_cast< bool*>(_v)); break;
        case 17: _t->setPriority(*reinterpret_cast< Priority*>(_v)); break;
        default: break;
        }
    }
if (_c == QMetaObject::ResetProperty) {
        switch (_id) {
        case 2: _t->resetEnabled(); break;
        default: break;
        }
    }
}

const QMetaObject *QAction::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QAction::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN7QActionE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int QAction::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 16;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    return _id;
}

// SIGNAL 0
void QAction::changed()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void QAction::enabledChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QAction::checkableChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QAction::visibleChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void QAction::triggered(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 6
void QAction::hovered()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void QAction::toggled(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}
QT_WARNING_POP
