/****************************************************************************
** Meta object code from reading C++ file 'qchronotimer.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/corelib/kernel/qchronotimer.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QProperty>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qchronotimer.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN12QChronoTimerE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN12QChronoTimerE = QtMocHelpers::stringData(
    "QChronoTimer",
    "timeout",
    "",
    "start",
    "stop",
    "singleShot",
    "interval",
    "std::chrono::nanoseconds",
    "remainingTime",
    "timerType",
    "Qt::TimerType",
    "active"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN12QChronoTimerE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       5,   35, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   32,    2, 0x06,    6 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       3,    0,   33,    2, 0x0a,    7 /* Public */,
       4,    0,   34,    2, 0x0a,    8 /* Public */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,

 // properties: name, type, flags, notifyId, revision
       5, QMetaType::Bool, 0x02015903, uint(-1), 0,
       6, 0x80000000 | 7, 0x0201590b, uint(-1), 0,
       8, 0x80000000 | 7, 0x00015809, uint(-1), 0,
       9, 0x80000000 | 10, 0x0201590b, uint(-1), 0,
      11, QMetaType::Bool, 0x02005801, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QChronoTimer::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN12QChronoTimerE.offsetsAndSizes,
    qt_meta_data_ZN12QChronoTimerE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN12QChronoTimerE_t,
        // property 'singleShot'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'interval'
        QtPrivate::TypeAndForceComplete<std::chrono::nanoseconds, std::true_type>,
        // property 'remainingTime'
        QtPrivate::TypeAndForceComplete<std::chrono::nanoseconds, std::true_type>,
        // property 'timerType'
        QtPrivate::TypeAndForceComplete<Qt::TimerType, std::true_type>,
        // property 'active'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QChronoTimer, std::true_type>,
        // method 'timeout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'start'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'stop'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void QChronoTimer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QChronoTimer *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->timeout(QPrivateSignal()); break;
        case 1: _t->start(); break;
        case 2: _t->stop(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QChronoTimer::*)(QPrivateSignal);
            if (_q_method_type _q_method = &QChronoTimer::timeout; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isSingleShot(); break;
        case 1: *reinterpret_cast< std::chrono::nanoseconds*>(_v) = _t->interval(); break;
        case 2: *reinterpret_cast< std::chrono::nanoseconds*>(_v) = _t->remainingTime(); break;
        case 3: *reinterpret_cast< Qt::TimerType*>(_v) = _t->timerType(); break;
        case 4: *reinterpret_cast< bool*>(_v) = _t->isActive(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setSingleShot(*reinterpret_cast< bool*>(_v)); break;
        case 1: _t->setInterval(*reinterpret_cast< std::chrono::nanoseconds*>(_v)); break;
        case 3: _t->setTimerType(*reinterpret_cast< Qt::TimerType*>(_v)); break;
        default: break;
        }
    }
    if (_c == QMetaObject::BindableProperty) {
        switch (_id) {
        case 0: *static_cast<QUntypedBindable *>(_a[0]) = _t->bindableSingleShot(); break;
        case 1: *static_cast<QUntypedBindable *>(_a[0]) = _t->bindableInterval(); break;
        case 3: *static_cast<QUntypedBindable *>(_a[0]) = _t->bindableTimerType(); break;
        case 4: *static_cast<QUntypedBindable *>(_a[0]) = _t->bindableActive(); break;
        default: break;
        }
    }
}

const QMetaObject *QChronoTimer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QChronoTimer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN12QChronoTimerE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int QChronoTimer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
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
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void QChronoTimer::timeout(QPrivateSignal _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
