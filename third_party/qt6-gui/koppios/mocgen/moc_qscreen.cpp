/****************************************************************************
** Meta object code from reading C++ file 'qscreen.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/kernel/qscreen.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qscreen.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7QScreenE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN7QScreenE = QtMocHelpers::stringData(
    "QScreen",
    "geometryChanged",
    "",
    "geometry",
    "availableGeometryChanged",
    "physicalSizeChanged",
    "size",
    "physicalDotsPerInchChanged",
    "dpi",
    "logicalDotsPerInchChanged",
    "virtualGeometryChanged",
    "rect",
    "primaryOrientationChanged",
    "Qt::ScreenOrientation",
    "orientation",
    "orientationChanged",
    "refreshRateChanged",
    "refreshRate",
    "name",
    "manufacturer",
    "model",
    "serialNumber",
    "depth",
    "availableSize",
    "virtualSize",
    "availableVirtualSize",
    "availableGeometry",
    "virtualGeometry",
    "availableVirtualGeometry",
    "physicalSize",
    "physicalDotsPerInchX",
    "physicalDotsPerInchY",
    "physicalDotsPerInch",
    "logicalDotsPerInchX",
    "logicalDotsPerInchY",
    "logicalDotsPerInch",
    "devicePixelRatio",
    "primaryOrientation",
    "nativeOrientation"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN7QScreenE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
      25,   95, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       9,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   68,    2, 0x06,   26 /* Public */,
       4,    1,   71,    2, 0x06,   28 /* Public */,
       5,    1,   74,    2, 0x06,   30 /* Public */,
       7,    1,   77,    2, 0x06,   32 /* Public */,
       9,    1,   80,    2, 0x06,   34 /* Public */,
      10,    1,   83,    2, 0x06,   36 /* Public */,
      12,    1,   86,    2, 0x06,   38 /* Public */,
      15,    1,   89,    2, 0x06,   40 /* Public */,
      16,    1,   92,    2, 0x06,   42 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QRect,    3,
    QMetaType::Void, QMetaType::QRect,    3,
    QMetaType::Void, QMetaType::QSizeF,    6,
    QMetaType::Void, QMetaType::QReal,    8,
    QMetaType::Void, QMetaType::QReal,    8,
    QMetaType::Void, QMetaType::QRect,   11,
    QMetaType::Void, 0x80000000 | 13,   14,
    QMetaType::Void, 0x80000000 | 13,   14,
    QMetaType::Void, QMetaType::QReal,   17,

 // properties: name, type, flags, notifyId, revision
      18, QMetaType::QString, 0x00015401, uint(-1), 0,
      19, QMetaType::QString, 0x00015401, uint(-1), 0,
      20, QMetaType::QString, 0x00015401, uint(-1), 0,
      21, QMetaType::QString, 0x00015401, uint(-1), 0,
      22, QMetaType::Int, 0x00015401, uint(-1), 0,
       6, QMetaType::QSize, 0x00015001, uint(0), 0,
      23, QMetaType::QSize, 0x00015001, uint(1), 0,
      24, QMetaType::QSize, 0x00015001, uint(5), 0,
      25, QMetaType::QSize, 0x00015001, uint(5), 0,
       3, QMetaType::QRect, 0x00015001, uint(0), 0,
      26, QMetaType::QRect, 0x00015001, uint(1), 0,
      27, QMetaType::QRect, 0x00015001, uint(5), 0,
      28, QMetaType::QRect, 0x00015001, uint(5), 0,
      29, QMetaType::QSizeF, 0x00015001, uint(2), 0,
      30, QMetaType::QReal, 0x00015001, uint(3), 0,
      31, QMetaType::QReal, 0x00015001, uint(3), 0,
      32, QMetaType::QReal, 0x00015001, uint(3), 0,
      33, QMetaType::QReal, 0x00015001, uint(4), 0,
      34, QMetaType::QReal, 0x00015001, uint(4), 0,
      35, QMetaType::QReal, 0x00015001, uint(4), 0,
      36, QMetaType::QReal, 0x00015001, uint(3), 0,
      37, 0x80000000 | 13, 0x00015009, uint(6), 0,
      14, 0x80000000 | 13, 0x00015009, uint(7), 0,
      38, 0x80000000 | 13, 0x00015009, uint(-1), 0,
      17, QMetaType::QReal, 0x00015001, uint(8), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN7QScreenE.offsetsAndSizes,
    qt_meta_data_ZN7QScreenE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN7QScreenE_t,
        // property 'name'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'manufacturer'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'model'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'serialNumber'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'depth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'size'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'availableSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'virtualSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'availableVirtualSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'geometry'
        QtPrivate::TypeAndForceComplete<QRect, std::true_type>,
        // property 'availableGeometry'
        QtPrivate::TypeAndForceComplete<QRect, std::true_type>,
        // property 'virtualGeometry'
        QtPrivate::TypeAndForceComplete<QRect, std::true_type>,
        // property 'availableVirtualGeometry'
        QtPrivate::TypeAndForceComplete<QRect, std::true_type>,
        // property 'physicalSize'
        QtPrivate::TypeAndForceComplete<QSizeF, std::true_type>,
        // property 'physicalDotsPerInchX'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'physicalDotsPerInchY'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'physicalDotsPerInch'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'logicalDotsPerInchX'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'logicalDotsPerInchY'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'logicalDotsPerInch'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'devicePixelRatio'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'primaryOrientation'
        QtPrivate::TypeAndForceComplete<Qt::ScreenOrientation, std::true_type>,
        // property 'orientation'
        QtPrivate::TypeAndForceComplete<Qt::ScreenOrientation, std::true_type>,
        // property 'nativeOrientation'
        QtPrivate::TypeAndForceComplete<Qt::ScreenOrientation, std::true_type>,
        // property 'refreshRate'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QScreen, std::true_type>,
        // method 'geometryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QRect &, std::false_type>,
        // method 'availableGeometryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QRect &, std::false_type>,
        // method 'physicalSizeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QSizeF &, std::false_type>,
        // method 'physicalDotsPerInchChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        // method 'logicalDotsPerInchChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        // method 'virtualGeometryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QRect &, std::false_type>,
        // method 'primaryOrientationChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::ScreenOrientation, std::false_type>,
        // method 'orientationChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::ScreenOrientation, std::false_type>,
        // method 'refreshRateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>
    >,
    nullptr
} };

void QScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QScreen *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->geometryChanged((*reinterpret_cast< std::add_pointer_t<QRect>>(_a[1]))); break;
        case 1: _t->availableGeometryChanged((*reinterpret_cast< std::add_pointer_t<QRect>>(_a[1]))); break;
        case 2: _t->physicalSizeChanged((*reinterpret_cast< std::add_pointer_t<QSizeF>>(_a[1]))); break;
        case 3: _t->physicalDotsPerInchChanged((*reinterpret_cast< std::add_pointer_t<qreal>>(_a[1]))); break;
        case 4: _t->logicalDotsPerInchChanged((*reinterpret_cast< std::add_pointer_t<qreal>>(_a[1]))); break;
        case 5: _t->virtualGeometryChanged((*reinterpret_cast< std::add_pointer_t<QRect>>(_a[1]))); break;
        case 6: _t->primaryOrientationChanged((*reinterpret_cast< std::add_pointer_t<Qt::ScreenOrientation>>(_a[1]))); break;
        case 7: _t->orientationChanged((*reinterpret_cast< std::add_pointer_t<Qt::ScreenOrientation>>(_a[1]))); break;
        case 8: _t->refreshRateChanged((*reinterpret_cast< std::add_pointer_t<qreal>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QScreen::*)(const QRect & );
            if (_q_method_type _q_method = &QScreen::geometryChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QScreen::*)(const QRect & );
            if (_q_method_type _q_method = &QScreen::availableGeometryChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QScreen::*)(const QSizeF & );
            if (_q_method_type _q_method = &QScreen::physicalSizeChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QScreen::*)(qreal );
            if (_q_method_type _q_method = &QScreen::physicalDotsPerInchChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QScreen::*)(qreal );
            if (_q_method_type _q_method = &QScreen::logicalDotsPerInchChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (QScreen::*)(const QRect & );
            if (_q_method_type _q_method = &QScreen::virtualGeometryChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (QScreen::*)(Qt::ScreenOrientation );
            if (_q_method_type _q_method = &QScreen::primaryOrientationChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (QScreen::*)(Qt::ScreenOrientation );
            if (_q_method_type _q_method = &QScreen::orientationChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (QScreen::*)(qreal );
            if (_q_method_type _q_method = &QScreen::refreshRateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->name(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->manufacturer(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->model(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->serialNumber(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->depth(); break;
        case 5: *reinterpret_cast< QSize*>(_v) = _t->size(); break;
        case 6: *reinterpret_cast< QSize*>(_v) = _t->availableSize(); break;
        case 7: *reinterpret_cast< QSize*>(_v) = _t->virtualSize(); break;
        case 8: *reinterpret_cast< QSize*>(_v) = _t->availableVirtualSize(); break;
        case 9: *reinterpret_cast< QRect*>(_v) = _t->geometry(); break;
        case 10: *reinterpret_cast< QRect*>(_v) = _t->availableGeometry(); break;
        case 11: *reinterpret_cast< QRect*>(_v) = _t->virtualGeometry(); break;
        case 12: *reinterpret_cast< QRect*>(_v) = _t->availableVirtualGeometry(); break;
        case 13: *reinterpret_cast< QSizeF*>(_v) = _t->physicalSize(); break;
        case 14: *reinterpret_cast< qreal*>(_v) = _t->physicalDotsPerInchX(); break;
        case 15: *reinterpret_cast< qreal*>(_v) = _t->physicalDotsPerInchY(); break;
        case 16: *reinterpret_cast< qreal*>(_v) = _t->physicalDotsPerInch(); break;
        case 17: *reinterpret_cast< qreal*>(_v) = _t->logicalDotsPerInchX(); break;
        case 18: *reinterpret_cast< qreal*>(_v) = _t->logicalDotsPerInchY(); break;
        case 19: *reinterpret_cast< qreal*>(_v) = _t->logicalDotsPerInch(); break;
        case 20: *reinterpret_cast< qreal*>(_v) = _t->devicePixelRatio(); break;
        case 21: *reinterpret_cast< Qt::ScreenOrientation*>(_v) = _t->primaryOrientation(); break;
        case 22: *reinterpret_cast< Qt::ScreenOrientation*>(_v) = _t->orientation(); break;
        case 23: *reinterpret_cast< Qt::ScreenOrientation*>(_v) = _t->nativeOrientation(); break;
        case 24: *reinterpret_cast< qreal*>(_v) = _t->refreshRate(); break;
        default: break;
        }
    }
}

const QMetaObject *QScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN7QScreenE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int QScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
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
        _id -= 25;
    }
    return _id;
}

// SIGNAL 0
void QScreen::geometryChanged(const QRect & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QScreen::availableGeometryChanged(const QRect & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QScreen::physicalSizeChanged(const QSizeF & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QScreen::physicalDotsPerInchChanged(qreal _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void QScreen::logicalDotsPerInchChanged(qreal _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void QScreen::virtualGeometryChanged(const QRect & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void QScreen::primaryOrientationChanged(Qt::ScreenOrientation _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void QScreen::orientationChanged(Qt::ScreenOrientation _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void QScreen::refreshRateChanged(qreal _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}
QT_WARNING_POP
