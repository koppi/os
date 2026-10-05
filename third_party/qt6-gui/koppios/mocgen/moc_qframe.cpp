/****************************************************************************
** Meta object code from reading C++ file 'qframe.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qframe.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qframe.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN6QFrameE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN6QFrameE = QtMocHelpers::stringData(
    "QFrame",
    "frameShape",
    "Shape",
    "frameShadow",
    "Shadow",
    "lineWidth",
    "midLineWidth",
    "frameWidth",
    "frameRect",
    "NoFrame",
    "Box",
    "Panel",
    "WinPanel",
    "HLine",
    "VLine",
    "StyledPanel",
    "Plain",
    "Raised",
    "Sunken"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN6QFrameE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       6,   14, // properties
       2,   44, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, 0x80000000 | 2, 0x0001510b, uint(-1), 0,
       3, 0x80000000 | 4, 0x0001510b, uint(-1), 0,
       5, QMetaType::Int, 0x00015103, uint(-1), 0,
       6, QMetaType::Int, 0x00015103, uint(-1), 0,
       7, QMetaType::Int, 0x00015001, uint(-1), 0,
       8, QMetaType::QRect, 0x00014103, uint(-1), 0,

 // enums: name, alias, flags, count, data
       2,    2, 0x0,    7,   54,
       4,    4, 0x0,    3,   68,

 // enum data: key, value
       9, uint(QFrame::NoFrame),
      10, uint(QFrame::Box),
      11, uint(QFrame::Panel),
      12, uint(QFrame::WinPanel),
      13, uint(QFrame::HLine),
      14, uint(QFrame::VLine),
      15, uint(QFrame::StyledPanel),
      16, uint(QFrame::Plain),
      17, uint(QFrame::Raised),
      18, uint(QFrame::Sunken),

       0        // eod
};

Q_CONSTINIT const QMetaObject QFrame::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN6QFrameE.offsetsAndSizes,
    qt_meta_data_ZN6QFrameE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN6QFrameE_t,
        // property 'frameShape'
        QtPrivate::TypeAndForceComplete<Shape, std::true_type>,
        // property 'frameShadow'
        QtPrivate::TypeAndForceComplete<Shadow, std::true_type>,
        // property 'lineWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'midLineWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'frameWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'frameRect'
        QtPrivate::TypeAndForceComplete<QRect, std::true_type>,
        // enum 'Shape'
        QtPrivate::TypeAndForceComplete<QFrame::Shape, std::true_type>,
        // enum 'Shadow'
        QtPrivate::TypeAndForceComplete<QFrame::Shadow, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QFrame, std::true_type>
    >,
    nullptr
} };

void QFrame::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QFrame *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< Shape*>(_v) = _t->frameShape(); break;
        case 1: *reinterpret_cast< Shadow*>(_v) = _t->frameShadow(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->lineWidth(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->midLineWidth(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->frameWidth(); break;
        case 5: *reinterpret_cast< QRect*>(_v) = _t->frameRect(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setFrameShape(*reinterpret_cast< Shape*>(_v)); break;
        case 1: _t->setFrameShadow(*reinterpret_cast< Shadow*>(_v)); break;
        case 2: _t->setLineWidth(*reinterpret_cast< int*>(_v)); break;
        case 3: _t->setMidLineWidth(*reinterpret_cast< int*>(_v)); break;
        case 5: _t->setFrameRect(*reinterpret_cast< QRect*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QFrame::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QFrame::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN6QFrameE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QFrame::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}
QT_WARNING_POP
