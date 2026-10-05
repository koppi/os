/****************************************************************************
** Meta object code from reading C++ file 'qlcdnumber.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qlcdnumber.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qlcdnumber.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10QLCDNumberE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10QLCDNumberE = QtMocHelpers::stringData(
    "QLCDNumber",
    "overflow",
    "",
    "display",
    "str",
    "num",
    "setHexMode",
    "setDecMode",
    "setOctMode",
    "setBinMode",
    "setSmallDecimalPoint",
    "smallDecimalPoint",
    "digitCount",
    "mode",
    "Mode",
    "segmentStyle",
    "SegmentStyle",
    "value",
    "intValue",
    "Hex",
    "Dec",
    "Oct",
    "Bin",
    "Outline",
    "Filled",
    "Flat"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10QLCDNumberE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       6,   85, // properties
       2,  115, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   68,    2, 0x06,    9 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       3,    1,   69,    2, 0x0a,   10 /* Public */,
       3,    1,   72,    2, 0x0a,   12 /* Public */,
       3,    1,   75,    2, 0x0a,   14 /* Public */,
       6,    0,   78,    2, 0x0a,   16 /* Public */,
       7,    0,   79,    2, 0x0a,   17 /* Public */,
       8,    0,   80,    2, 0x0a,   18 /* Public */,
       9,    0,   81,    2, 0x0a,   19 /* Public */,
      10,    1,   82,    2, 0x0a,   20 /* Public */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,    4,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::Double,    5,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    2,

 // properties: name, type, flags, notifyId, revision
      11, QMetaType::Bool, 0x00015103, uint(-1), 0,
      12, QMetaType::Int, 0x00015103, uint(-1), 0,
      13, 0x80000000 | 14, 0x0001510b, uint(-1), 0,
      15, 0x80000000 | 16, 0x0001510b, uint(-1), 0,
      17, QMetaType::Double, 0x00015003, uint(-1), 0,
      18, QMetaType::Int, 0x00015003, uint(-1), 0,

 // enums: name, alias, flags, count, data
      14,   14, 0x0,    4,  125,
      16,   16, 0x0,    3,  133,

 // enum data: key, value
      19, uint(QLCDNumber::Hex),
      20, uint(QLCDNumber::Dec),
      21, uint(QLCDNumber::Oct),
      22, uint(QLCDNumber::Bin),
      23, uint(QLCDNumber::Outline),
      24, uint(QLCDNumber::Filled),
      25, uint(QLCDNumber::Flat),

       0        // eod
};

Q_CONSTINIT const QMetaObject QLCDNumber::staticMetaObject = { {
    QMetaObject::SuperData::link<QFrame::staticMetaObject>(),
    qt_meta_stringdata_ZN10QLCDNumberE.offsetsAndSizes,
    qt_meta_data_ZN10QLCDNumberE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10QLCDNumberE_t,
        // property 'smallDecimalPoint'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'digitCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'mode'
        QtPrivate::TypeAndForceComplete<Mode, std::true_type>,
        // property 'segmentStyle'
        QtPrivate::TypeAndForceComplete<SegmentStyle, std::true_type>,
        // property 'value'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'intValue'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // enum 'Mode'
        QtPrivate::TypeAndForceComplete<QLCDNumber::Mode, std::true_type>,
        // enum 'SegmentStyle'
        QtPrivate::TypeAndForceComplete<QLCDNumber::SegmentStyle, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QLCDNumber, std::true_type>,
        // method 'overflow'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'display'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'display'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'display'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'setHexMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setDecMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setOctMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setBinMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setSmallDecimalPoint'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void QLCDNumber::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QLCDNumber *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->overflow(); break;
        case 1: _t->display((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->display((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->display((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 4: _t->setHexMode(); break;
        case 5: _t->setDecMode(); break;
        case 6: _t->setOctMode(); break;
        case 7: _t->setBinMode(); break;
        case 8: _t->setSmallDecimalPoint((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QLCDNumber::*)();
            if (_q_method_type _q_method = &QLCDNumber::overflow; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->smallDecimalPoint(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->digitCount(); break;
        case 2: *reinterpret_cast< Mode*>(_v) = _t->mode(); break;
        case 3: *reinterpret_cast< SegmentStyle*>(_v) = _t->segmentStyle(); break;
        case 4: *reinterpret_cast< double*>(_v) = _t->value(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->intValue(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setSmallDecimalPoint(*reinterpret_cast< bool*>(_v)); break;
        case 1: _t->setDigitCount(*reinterpret_cast< int*>(_v)); break;
        case 2: _t->setMode(*reinterpret_cast< Mode*>(_v)); break;
        case 3: _t->setSegmentStyle(*reinterpret_cast< SegmentStyle*>(_v)); break;
        case 4: _t->display(*reinterpret_cast< double*>(_v)); break;
        case 5: _t->display(*reinterpret_cast< int*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QLCDNumber::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QLCDNumber::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN10QLCDNumberE.stringdata0))
        return static_cast<void*>(this);
    return QFrame::qt_metacast(_clname);
}

int QLCDNumber::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QFrame::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void QLCDNumber::overflow()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
