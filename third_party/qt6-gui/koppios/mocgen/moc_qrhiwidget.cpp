/****************************************************************************
** Meta object code from reading C++ file 'qrhiwidget.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/kernel/qrhiwidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qrhiwidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10QRhiWidgetE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10QRhiWidgetE = QtMocHelpers::stringData(
    "QRhiWidget",
    "frameSubmitted",
    "",
    "renderFailed",
    "sampleCountChanged",
    "samples",
    "colorBufferFormatChanged",
    "TextureFormat",
    "format",
    "fixedColorBufferSizeChanged",
    "pixelSize",
    "mirrorVerticallyChanged",
    "enabled",
    "sampleCount",
    "colorBufferFormat",
    "fixedColorBufferSize",
    "mirrorVertically",
    "Api",
    "Null",
    "OpenGL",
    "Metal",
    "Vulkan",
    "Direct3D11",
    "Direct3D12",
    "RGBA8",
    "RGBA16F",
    "RGBA32F",
    "RGB10A2"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10QRhiWidgetE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       4,   64, // properties
       2,   84, // enums/sets
       0,    0, // constructors
       0,       // flags
       6,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   50,    2, 0x06,    7 /* Public */,
       3,    0,   51,    2, 0x06,    8 /* Public */,
       4,    1,   52,    2, 0x06,    9 /* Public */,
       6,    1,   55,    2, 0x06,   11 /* Public */,
       9,    1,   58,    2, 0x06,   13 /* Public */,
      11,    1,   61,    2, 0x06,   15 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, 0x80000000 | 7,    8,
    QMetaType::Void, QMetaType::QSize,   10,
    QMetaType::Void, QMetaType::Bool,   12,

 // properties: name, type, flags, notifyId, revision
      13, QMetaType::Int, 0x00015103, uint(2), 0,
      14, 0x80000000 | 7, 0x0001510b, uint(3), 0,
      15, QMetaType::QSize, 0x00015103, uint(4), 0,
      16, QMetaType::Bool, 0x00015103, uint(5), 0,

 // enums: name, alias, flags, count, data
      17,   17, 0x2,    6,   94,
       7,    7, 0x2,    4,  106,

 // enum data: key, value
      18, uint(QRhiWidget::Api::Null),
      19, uint(QRhiWidget::Api::OpenGL),
      20, uint(QRhiWidget::Api::Metal),
      21, uint(QRhiWidget::Api::Vulkan),
      22, uint(QRhiWidget::Api::Direct3D11),
      23, uint(QRhiWidget::Api::Direct3D12),
      24, uint(QRhiWidget::TextureFormat::RGBA8),
      25, uint(QRhiWidget::TextureFormat::RGBA16F),
      26, uint(QRhiWidget::TextureFormat::RGBA32F),
      27, uint(QRhiWidget::TextureFormat::RGB10A2),

       0        // eod
};

Q_CONSTINIT const QMetaObject QRhiWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN10QRhiWidgetE.offsetsAndSizes,
    qt_meta_data_ZN10QRhiWidgetE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10QRhiWidgetE_t,
        // property 'sampleCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'colorBufferFormat'
        QtPrivate::TypeAndForceComplete<TextureFormat, std::true_type>,
        // property 'fixedColorBufferSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'mirrorVertically'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // enum 'Api'
        QtPrivate::TypeAndForceComplete<QRhiWidget::Api, std::true_type>,
        // enum 'TextureFormat'
        QtPrivate::TypeAndForceComplete<QRhiWidget::TextureFormat, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QRhiWidget, std::true_type>,
        // method 'frameSubmitted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'renderFailed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'sampleCountChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'colorBufferFormatChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<TextureFormat, std::false_type>,
        // method 'fixedColorBufferSizeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QSize &, std::false_type>,
        // method 'mirrorVerticallyChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void QRhiWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QRhiWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->frameSubmitted(); break;
        case 1: _t->renderFailed(); break;
        case 2: _t->sampleCountChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->colorBufferFormatChanged((*reinterpret_cast< std::add_pointer_t<TextureFormat>>(_a[1]))); break;
        case 4: _t->fixedColorBufferSizeChanged((*reinterpret_cast< std::add_pointer_t<QSize>>(_a[1]))); break;
        case 5: _t->mirrorVerticallyChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QRhiWidget::*)();
            if (_q_method_type _q_method = &QRhiWidget::frameSubmitted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QRhiWidget::*)();
            if (_q_method_type _q_method = &QRhiWidget::renderFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QRhiWidget::*)(int );
            if (_q_method_type _q_method = &QRhiWidget::sampleCountChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QRhiWidget::*)(TextureFormat );
            if (_q_method_type _q_method = &QRhiWidget::colorBufferFormatChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QRhiWidget::*)(const QSize & );
            if (_q_method_type _q_method = &QRhiWidget::fixedColorBufferSizeChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (QRhiWidget::*)(bool );
            if (_q_method_type _q_method = &QRhiWidget::mirrorVerticallyChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = _t->sampleCount(); break;
        case 1: *reinterpret_cast< TextureFormat*>(_v) = _t->colorBufferFormat(); break;
        case 2: *reinterpret_cast< QSize*>(_v) = _t->fixedColorBufferSize(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->isMirrorVerticallyEnabled(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setSampleCount(*reinterpret_cast< int*>(_v)); break;
        case 1: _t->setColorBufferFormat(*reinterpret_cast< TextureFormat*>(_v)); break;
        case 2: _t->setFixedColorBufferSize(*reinterpret_cast< QSize*>(_v)); break;
        case 3: _t->setMirrorVertically(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QRhiWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QRhiWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN10QRhiWidgetE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QRhiWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void QRhiWidget::frameSubmitted()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void QRhiWidget::renderFailed()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void QRhiWidget::sampleCountChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QRhiWidget::colorBufferFormatChanged(TextureFormat _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void QRhiWidget::fixedColorBufferSizeChanged(const QSize & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void QRhiWidget::mirrorVerticallyChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}
QT_WARNING_POP
