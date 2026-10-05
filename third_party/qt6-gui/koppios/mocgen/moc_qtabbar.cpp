/****************************************************************************
** Meta object code from reading C++ file 'qtabbar.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qtabbar.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qtabbar.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7QTabBarE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN7QTabBarE = QtMocHelpers::stringData(
    "QTabBar",
    "currentChanged",
    "",
    "index",
    "tabCloseRequested",
    "tabMoved",
    "from",
    "to",
    "tabBarClicked",
    "tabBarDoubleClicked",
    "setCurrentIndex",
    "shape",
    "Shape",
    "currentIndex",
    "count",
    "drawBase",
    "iconSize",
    "elideMode",
    "Qt::TextElideMode",
    "usesScrollButtons",
    "tabsClosable",
    "selectionBehaviorOnRemove",
    "SelectionBehavior",
    "expanding",
    "movable",
    "documentMode",
    "autoHide",
    "changeCurrentOnDrag",
    "RoundedNorth",
    "RoundedSouth",
    "RoundedWest",
    "RoundedEast",
    "TriangularNorth",
    "TriangularSouth",
    "TriangularWest",
    "TriangularEast"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN7QTabBarE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
      14,   70, // properties
       1,  140, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   50,    2, 0x06,   16 /* Public */,
       4,    1,   53,    2, 0x06,   18 /* Public */,
       5,    2,   56,    2, 0x06,   20 /* Public */,
       8,    1,   61,    2, 0x06,   23 /* Public */,
       9,    1,   64,    2, 0x06,   25 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      10,    1,   67,    2, 0x0a,   27 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    6,    7,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    3,

 // properties: name, type, flags, notifyId, revision
      11, 0x80000000 | 12, 0x0001510b, uint(-1), 0,
      13, QMetaType::Int, 0x00015103, uint(0), 0,
      14, QMetaType::Int, 0x00015001, uint(-1), 0,
      15, QMetaType::Bool, 0x00015103, uint(-1), 0,
      16, QMetaType::QSize, 0x00015103, uint(-1), 0,
      17, 0x80000000 | 18, 0x0001510b, uint(-1), 0,
      19, QMetaType::Bool, 0x00015103, uint(-1), 0,
      20, QMetaType::Bool, 0x00015103, uint(-1), 0,
      21, 0x80000000 | 22, 0x0001510b, uint(-1), 0,
      23, QMetaType::Bool, 0x00015103, uint(-1), 0,
      24, QMetaType::Bool, 0x00015103, uint(-1), 0,
      25, QMetaType::Bool, 0x00015103, uint(-1), 0,
      26, QMetaType::Bool, 0x00015103, uint(-1), 0,
      27, QMetaType::Bool, 0x00015103, uint(-1), 0,

 // enums: name, alias, flags, count, data
      12,   12, 0x0,    8,  145,

 // enum data: key, value
      28, uint(QTabBar::RoundedNorth),
      29, uint(QTabBar::RoundedSouth),
      30, uint(QTabBar::RoundedWest),
      31, uint(QTabBar::RoundedEast),
      32, uint(QTabBar::TriangularNorth),
      33, uint(QTabBar::TriangularSouth),
      34, uint(QTabBar::TriangularWest),
      35, uint(QTabBar::TriangularEast),

       0        // eod
};

Q_CONSTINIT const QMetaObject QTabBar::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN7QTabBarE.offsetsAndSizes,
    qt_meta_data_ZN7QTabBarE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN7QTabBarE_t,
        // property 'shape'
        QtPrivate::TypeAndForceComplete<Shape, std::true_type>,
        // property 'currentIndex'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'count'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'drawBase'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'iconSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'elideMode'
        QtPrivate::TypeAndForceComplete<Qt::TextElideMode, std::true_type>,
        // property 'usesScrollButtons'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'tabsClosable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'selectionBehaviorOnRemove'
        QtPrivate::TypeAndForceComplete<SelectionBehavior, std::true_type>,
        // property 'expanding'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'movable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'documentMode'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'autoHide'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'changeCurrentOnDrag'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // enum 'Shape'
        QtPrivate::TypeAndForceComplete<QTabBar::Shape, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QTabBar, std::true_type>,
        // method 'currentChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'tabCloseRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'tabMoved'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'tabBarClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'tabBarDoubleClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setCurrentIndex'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>
    >,
    nullptr
} };

void QTabBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QTabBar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->currentChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->tabCloseRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->tabMoved((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 3: _t->tabBarClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->tabBarDoubleClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->setCurrentIndex((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QTabBar::*)(int );
            if (_q_method_type _q_method = &QTabBar::currentChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QTabBar::*)(int );
            if (_q_method_type _q_method = &QTabBar::tabCloseRequested; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QTabBar::*)(int , int );
            if (_q_method_type _q_method = &QTabBar::tabMoved; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QTabBar::*)(int );
            if (_q_method_type _q_method = &QTabBar::tabBarClicked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QTabBar::*)(int );
            if (_q_method_type _q_method = &QTabBar::tabBarDoubleClicked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< Shape*>(_v) = _t->shape(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->currentIndex(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->count(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->drawBase(); break;
        case 4: *reinterpret_cast< QSize*>(_v) = _t->iconSize(); break;
        case 5: *reinterpret_cast< Qt::TextElideMode*>(_v) = _t->elideMode(); break;
        case 6: *reinterpret_cast< bool*>(_v) = _t->usesScrollButtons(); break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->tabsClosable(); break;
        case 8: *reinterpret_cast< SelectionBehavior*>(_v) = _t->selectionBehaviorOnRemove(); break;
        case 9: *reinterpret_cast< bool*>(_v) = _t->expanding(); break;
        case 10: *reinterpret_cast< bool*>(_v) = _t->isMovable(); break;
        case 11: *reinterpret_cast< bool*>(_v) = _t->documentMode(); break;
        case 12: *reinterpret_cast< bool*>(_v) = _t->autoHide(); break;
        case 13: *reinterpret_cast< bool*>(_v) = _t->changeCurrentOnDrag(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setShape(*reinterpret_cast< Shape*>(_v)); break;
        case 1: _t->setCurrentIndex(*reinterpret_cast< int*>(_v)); break;
        case 3: _t->setDrawBase(*reinterpret_cast< bool*>(_v)); break;
        case 4: _t->setIconSize(*reinterpret_cast< QSize*>(_v)); break;
        case 5: _t->setElideMode(*reinterpret_cast< Qt::TextElideMode*>(_v)); break;
        case 6: _t->setUsesScrollButtons(*reinterpret_cast< bool*>(_v)); break;
        case 7: _t->setTabsClosable(*reinterpret_cast< bool*>(_v)); break;
        case 8: _t->setSelectionBehaviorOnRemove(*reinterpret_cast< SelectionBehavior*>(_v)); break;
        case 9: _t->setExpanding(*reinterpret_cast< bool*>(_v)); break;
        case 10: _t->setMovable(*reinterpret_cast< bool*>(_v)); break;
        case 11: _t->setDocumentMode(*reinterpret_cast< bool*>(_v)); break;
        case 12: _t->setAutoHide(*reinterpret_cast< bool*>(_v)); break;
        case 13: _t->setChangeCurrentOnDrag(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QTabBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QTabBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN7QTabBarE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QTabBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    }
    return _id;
}

// SIGNAL 0
void QTabBar::currentChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QTabBar::tabCloseRequested(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QTabBar::tabMoved(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QTabBar::tabBarClicked(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void QTabBar::tabBarDoubleClicked(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}
QT_WARNING_POP
