/****************************************************************************
** Meta object code from reading C++ file 'qtabwidget.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qtabwidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qtabwidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10QTabWidgetE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10QTabWidgetE = QtMocHelpers::stringData(
    "QTabWidget",
    "currentChanged",
    "",
    "index",
    "tabCloseRequested",
    "tabBarClicked",
    "tabBarDoubleClicked",
    "setCurrentIndex",
    "setCurrentWidget",
    "QWidget*",
    "widget",
    "tabPosition",
    "TabPosition",
    "tabShape",
    "TabShape",
    "currentIndex",
    "count",
    "iconSize",
    "elideMode",
    "Qt::TextElideMode",
    "usesScrollButtons",
    "documentMode",
    "tabsClosable",
    "movable",
    "tabBarAutoHide",
    "North",
    "South",
    "West",
    "East",
    "Rounded",
    "Triangular"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10QTabWidgetE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
      11,   68, // properties
       2,  123, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   50,    2, 0x06,   14 /* Public */,
       4,    1,   53,    2, 0x06,   16 /* Public */,
       5,    1,   56,    2, 0x06,   18 /* Public */,
       6,    1,   59,    2, 0x06,   20 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       7,    1,   62,    2, 0x0a,   22 /* Public */,
       8,    1,   65,    2, 0x0a,   24 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, 0x80000000 | 9,   10,

 // properties: name, type, flags, notifyId, revision
      11, 0x80000000 | 12, 0x0001510b, uint(-1), 0,
      13, 0x80000000 | 14, 0x0001510b, uint(-1), 0,
      15, QMetaType::Int, 0x00015103, uint(0), 0,
      16, QMetaType::Int, 0x00015001, uint(-1), 0,
      17, QMetaType::QSize, 0x00015103, uint(-1), 0,
      18, 0x80000000 | 19, 0x0001510b, uint(-1), 0,
      20, QMetaType::Bool, 0x00015103, uint(-1), 0,
      21, QMetaType::Bool, 0x00015103, uint(-1), 0,
      22, QMetaType::Bool, 0x00015103, uint(-1), 0,
      23, QMetaType::Bool, 0x00015103, uint(-1), 0,
      24, QMetaType::Bool, 0x00015103, uint(-1), 0,

 // enums: name, alias, flags, count, data
      12,   12, 0x0,    4,  133,
      14,   14, 0x0,    2,  141,

 // enum data: key, value
      25, uint(QTabWidget::North),
      26, uint(QTabWidget::South),
      27, uint(QTabWidget::West),
      28, uint(QTabWidget::East),
      29, uint(QTabWidget::Rounded),
      30, uint(QTabWidget::Triangular),

       0        // eod
};

Q_CONSTINIT const QMetaObject QTabWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN10QTabWidgetE.offsetsAndSizes,
    qt_meta_data_ZN10QTabWidgetE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10QTabWidgetE_t,
        // property 'tabPosition'
        QtPrivate::TypeAndForceComplete<TabPosition, std::true_type>,
        // property 'tabShape'
        QtPrivate::TypeAndForceComplete<TabShape, std::true_type>,
        // property 'currentIndex'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'count'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'iconSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'elideMode'
        QtPrivate::TypeAndForceComplete<Qt::TextElideMode, std::true_type>,
        // property 'usesScrollButtons'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'documentMode'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'tabsClosable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'movable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'tabBarAutoHide'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // enum 'TabPosition'
        QtPrivate::TypeAndForceComplete<QTabWidget::TabPosition, std::true_type>,
        // enum 'TabShape'
        QtPrivate::TypeAndForceComplete<QTabWidget::TabShape, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QTabWidget, std::true_type>,
        // method 'currentChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'tabCloseRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'tabBarClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'tabBarDoubleClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setCurrentIndex'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setCurrentWidget'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QWidget *, std::false_type>
    >,
    nullptr
} };

void QTabWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QTabWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->currentChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->tabCloseRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->tabBarClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->tabBarDoubleClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->setCurrentIndex((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->setCurrentWidget((*reinterpret_cast< std::add_pointer_t<QWidget*>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 5:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QWidget* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QTabWidget::*)(int );
            if (_q_method_type _q_method = &QTabWidget::currentChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QTabWidget::*)(int );
            if (_q_method_type _q_method = &QTabWidget::tabCloseRequested; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QTabWidget::*)(int );
            if (_q_method_type _q_method = &QTabWidget::tabBarClicked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QTabWidget::*)(int );
            if (_q_method_type _q_method = &QTabWidget::tabBarDoubleClicked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< TabPosition*>(_v) = _t->tabPosition(); break;
        case 1: *reinterpret_cast< TabShape*>(_v) = _t->tabShape(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->currentIndex(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->count(); break;
        case 4: *reinterpret_cast< QSize*>(_v) = _t->iconSize(); break;
        case 5: *reinterpret_cast< Qt::TextElideMode*>(_v) = _t->elideMode(); break;
        case 6: *reinterpret_cast< bool*>(_v) = _t->usesScrollButtons(); break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->documentMode(); break;
        case 8: *reinterpret_cast< bool*>(_v) = _t->tabsClosable(); break;
        case 9: *reinterpret_cast< bool*>(_v) = _t->isMovable(); break;
        case 10: *reinterpret_cast< bool*>(_v) = _t->tabBarAutoHide(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setTabPosition(*reinterpret_cast< TabPosition*>(_v)); break;
        case 1: _t->setTabShape(*reinterpret_cast< TabShape*>(_v)); break;
        case 2: _t->setCurrentIndex(*reinterpret_cast< int*>(_v)); break;
        case 4: _t->setIconSize(*reinterpret_cast< QSize*>(_v)); break;
        case 5: _t->setElideMode(*reinterpret_cast< Qt::TextElideMode*>(_v)); break;
        case 6: _t->setUsesScrollButtons(*reinterpret_cast< bool*>(_v)); break;
        case 7: _t->setDocumentMode(*reinterpret_cast< bool*>(_v)); break;
        case 8: _t->setTabsClosable(*reinterpret_cast< bool*>(_v)); break;
        case 9: _t->setMovable(*reinterpret_cast< bool*>(_v)); break;
        case 10: _t->setTabBarAutoHide(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QTabWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QTabWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN10QTabWidgetE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QTabWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
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
void QTabWidget::currentChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QTabWidget::tabCloseRequested(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QTabWidget::tabBarClicked(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QTabWidget::tabBarDoubleClicked(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
