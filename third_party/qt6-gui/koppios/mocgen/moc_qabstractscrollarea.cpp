/****************************************************************************
** Meta object code from reading C++ file 'qabstractscrollarea.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qabstractscrollarea.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qabstractscrollarea.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN19QAbstractScrollAreaE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN19QAbstractScrollAreaE = QtMocHelpers::stringData(
    "QAbstractScrollArea",
    "_q_hslide",
    "",
    "_q_vslide",
    "_q_showOrHideScrollBars",
    "verticalScrollBarPolicy",
    "Qt::ScrollBarPolicy",
    "horizontalScrollBarPolicy",
    "sizeAdjustPolicy",
    "SizeAdjustPolicy",
    "AdjustIgnored",
    "AdjustToContentsOnFirstShow",
    "AdjustToContents"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN19QAbstractScrollAreaE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       3,   39, // properties
       1,   54, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   32,    2, 0x08,    5 /* Private */,
       3,    1,   35,    2, 0x08,    7 /* Private */,
       4,    0,   38,    2, 0x08,    9 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    2,
    QMetaType::Void, QMetaType::Int,    2,
    QMetaType::Void,

 // properties: name, type, flags, notifyId, revision
       5, 0x80000000 | 6, 0x0001510b, uint(-1), 0,
       7, 0x80000000 | 6, 0x0001510b, uint(-1), 0,
       8, 0x80000000 | 9, 0x0001510b, uint(-1), 0,

 // enums: name, alias, flags, count, data
       9,    9, 0x0,    3,   59,

 // enum data: key, value
      10, uint(QAbstractScrollArea::AdjustIgnored),
      11, uint(QAbstractScrollArea::AdjustToContentsOnFirstShow),
      12, uint(QAbstractScrollArea::AdjustToContents),

       0        // eod
};

Q_CONSTINIT const QMetaObject QAbstractScrollArea::staticMetaObject = { {
    QMetaObject::SuperData::link<QFrame::staticMetaObject>(),
    qt_meta_stringdata_ZN19QAbstractScrollAreaE.offsetsAndSizes,
    qt_meta_data_ZN19QAbstractScrollAreaE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN19QAbstractScrollAreaE_t,
        // property 'verticalScrollBarPolicy'
        QtPrivate::TypeAndForceComplete<Qt::ScrollBarPolicy, std::true_type>,
        // property 'horizontalScrollBarPolicy'
        QtPrivate::TypeAndForceComplete<Qt::ScrollBarPolicy, std::true_type>,
        // property 'sizeAdjustPolicy'
        QtPrivate::TypeAndForceComplete<SizeAdjustPolicy, std::true_type>,
        // enum 'SizeAdjustPolicy'
        QtPrivate::TypeAndForceComplete<QAbstractScrollArea::SizeAdjustPolicy, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QAbstractScrollArea, std::true_type>,
        // method '_q_hslide'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method '_q_vslide'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method '_q_showOrHideScrollBars'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void QAbstractScrollArea::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QAbstractScrollArea *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->d_func()->_q_hslide((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->d_func()->_q_vslide((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->d_func()->_q_showOrHideScrollBars(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< Qt::ScrollBarPolicy*>(_v) = _t->verticalScrollBarPolicy(); break;
        case 1: *reinterpret_cast< Qt::ScrollBarPolicy*>(_v) = _t->horizontalScrollBarPolicy(); break;
        case 2: *reinterpret_cast< SizeAdjustPolicy*>(_v) = _t->sizeAdjustPolicy(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setVerticalScrollBarPolicy(*reinterpret_cast< Qt::ScrollBarPolicy*>(_v)); break;
        case 1: _t->setHorizontalScrollBarPolicy(*reinterpret_cast< Qt::ScrollBarPolicy*>(_v)); break;
        case 2: _t->setSizeAdjustPolicy(*reinterpret_cast< SizeAdjustPolicy*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QAbstractScrollArea::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QAbstractScrollArea::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN19QAbstractScrollAreaE.stringdata0))
        return static_cast<void*>(this);
    return QFrame::qt_metacast(_clname);
}

int QAbstractScrollArea::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QFrame::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}
QT_WARNING_POP
