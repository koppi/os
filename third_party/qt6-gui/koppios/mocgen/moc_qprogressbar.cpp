/****************************************************************************
** Meta object code from reading C++ file 'qprogressbar.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qprogressbar.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qprogressbar.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN12QProgressBarE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN12QProgressBarE = QtMocHelpers::stringData(
    "QProgressBar",
    "valueChanged",
    "",
    "value",
    "reset",
    "setRange",
    "minimum",
    "maximum",
    "setMinimum",
    "setMaximum",
    "setValue",
    "setOrientation",
    "Qt::Orientation",
    "text",
    "alignment",
    "Qt::Alignment",
    "textVisible",
    "orientation",
    "invertedAppearance",
    "textDirection",
    "Direction",
    "format",
    "TopToBottom",
    "BottomToTop"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN12QProgressBarE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
      10,   77, // properties
       1,  127, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   56,    2, 0x06,   12 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       4,    0,   59,    2, 0x0a,   14 /* Public */,
       5,    2,   60,    2, 0x0a,   15 /* Public */,
       8,    1,   65,    2, 0x0a,   18 /* Public */,
       9,    1,   68,    2, 0x0a,   20 /* Public */,
      10,    1,   71,    2, 0x0a,   22 /* Public */,
      11,    1,   74,    2, 0x0a,   24 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int,    3,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    6,    7,
    QMetaType::Void, QMetaType::Int,    6,
    QMetaType::Void, QMetaType::Int,    7,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, 0x80000000 | 12,    2,

 // properties: name, type, flags, notifyId, revision
       6, QMetaType::Int, 0x00015103, uint(-1), 0,
       7, QMetaType::Int, 0x00015103, uint(-1), 0,
      13, QMetaType::QString, 0x00015001, uint(-1), 0,
       3, QMetaType::Int, 0x00015103, uint(0), 0,
      14, 0x80000000 | 15, 0x0001510b, uint(-1), 0,
      16, QMetaType::Bool, 0x00015103, uint(-1), 0,
      17, 0x80000000 | 12, 0x0001510b, uint(-1), 0,
      18, QMetaType::Bool, 0x00015103, uint(-1), 0,
      19, 0x80000000 | 20, 0x0001510b, uint(-1), 0,
      21, QMetaType::QString, 0x00015107, uint(-1), 0,

 // enums: name, alias, flags, count, data
      20,   20, 0x0,    2,  132,

 // enum data: key, value
      22, uint(QProgressBar::TopToBottom),
      23, uint(QProgressBar::BottomToTop),

       0        // eod
};

Q_CONSTINIT const QMetaObject QProgressBar::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN12QProgressBarE.offsetsAndSizes,
    qt_meta_data_ZN12QProgressBarE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN12QProgressBarE_t,
        // property 'minimum'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maximum'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'text'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'value'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'alignment'
        QtPrivate::TypeAndForceComplete<Qt::Alignment, std::true_type>,
        // property 'textVisible'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'orientation'
        QtPrivate::TypeAndForceComplete<Qt::Orientation, std::true_type>,
        // property 'invertedAppearance'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'textDirection'
        QtPrivate::TypeAndForceComplete<Direction, std::true_type>,
        // property 'format'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // enum 'Direction'
        QtPrivate::TypeAndForceComplete<QProgressBar::Direction, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QProgressBar, std::true_type>,
        // method 'valueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'reset'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setRange'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setMinimum'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setMaximum'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setValue'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setOrientation'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::Orientation, std::false_type>
    >,
    nullptr
} };

void QProgressBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QProgressBar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->valueChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->reset(); break;
        case 2: _t->setRange((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 3: _t->setMinimum((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->setMaximum((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->setValue((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->setOrientation((*reinterpret_cast< std::add_pointer_t<Qt::Orientation>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QProgressBar::*)(int );
            if (_q_method_type _q_method = &QProgressBar::valueChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = _t->minimum(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->maximum(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->text(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->value(); break;
        case 4: *reinterpret_cast< Qt::Alignment*>(_v) = _t->alignment(); break;
        case 5: *reinterpret_cast< bool*>(_v) = _t->isTextVisible(); break;
        case 6: *reinterpret_cast< Qt::Orientation*>(_v) = _t->orientation(); break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->invertedAppearance(); break;
        case 8: *reinterpret_cast< Direction*>(_v) = _t->textDirection(); break;
        case 9: *reinterpret_cast< QString*>(_v) = _t->format(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setMinimum(*reinterpret_cast< int*>(_v)); break;
        case 1: _t->setMaximum(*reinterpret_cast< int*>(_v)); break;
        case 3: _t->setValue(*reinterpret_cast< int*>(_v)); break;
        case 4: _t->setAlignment(*reinterpret_cast< Qt::Alignment*>(_v)); break;
        case 5: _t->setTextVisible(*reinterpret_cast< bool*>(_v)); break;
        case 6: _t->setOrientation(*reinterpret_cast< Qt::Orientation*>(_v)); break;
        case 7: _t->setInvertedAppearance(*reinterpret_cast< bool*>(_v)); break;
        case 8: _t->setTextDirection(*reinterpret_cast< Direction*>(_v)); break;
        case 9: _t->setFormat(*reinterpret_cast< QString*>(_v)); break;
        default: break;
        }
    }
if (_c == QMetaObject::ResetProperty) {
        switch (_id) {
        case 9: _t->resetFormat(); break;
        default: break;
        }
    }
}

const QMetaObject *QProgressBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QProgressBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN12QProgressBarE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QProgressBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void QProgressBar::valueChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
