/****************************************************************************
** Meta object code from reading C++ file 'qlistview.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/itemviews/qlistview.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qlistview.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9QListViewE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN9QListViewE = QtMocHelpers::stringData(
    "QListView",
    "indexesMoved",
    "",
    "QModelIndexList",
    "indexes",
    "movement",
    "Movement",
    "flow",
    "Flow",
    "isWrapping",
    "resizeMode",
    "ResizeMode",
    "layoutMode",
    "LayoutMode",
    "spacing",
    "gridSize",
    "viewMode",
    "ViewMode",
    "modelColumn",
    "uniformItemSizes",
    "batchSize",
    "wordWrap",
    "selectionRectVisible",
    "itemAlignment",
    "Qt::Alignment",
    "Static",
    "Free",
    "Snap",
    "LeftToRight",
    "TopToBottom",
    "Fixed",
    "Adjust",
    "SinglePass",
    "Batched",
    "ListMode",
    "IconMode"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN9QListViewE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
      14,   23, // properties
       5,   93, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   20,    2, 0x06,   20 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,

 // properties: name, type, flags, notifyId, revision
       5, 0x80000000 | 6, 0x0001510b, uint(-1), 0,
       7, 0x80000000 | 8, 0x0001510b, uint(-1), 0,
       9, QMetaType::Bool, 0x00015003, uint(-1), 0,
      10, 0x80000000 | 11, 0x0001510b, uint(-1), 0,
      12, 0x80000000 | 13, 0x0001510b, uint(-1), 0,
      14, QMetaType::Int, 0x00015103, uint(-1), 0,
      15, QMetaType::QSize, 0x00015103, uint(-1), 0,
      16, 0x80000000 | 17, 0x0001510b, uint(-1), 0,
      18, QMetaType::Int, 0x00015103, uint(-1), 0,
      19, QMetaType::Bool, 0x00015103, uint(-1), 0,
      20, QMetaType::Int, 0x00015103, uint(-1), 0,
      21, QMetaType::Bool, 0x00015103, uint(-1), 0,
      22, QMetaType::Bool, 0x00015103, uint(-1), 0,
      23, 0x80000000 | 24, 0x0001510b, uint(-1), 0,

 // enums: name, alias, flags, count, data
       6,    6, 0x0,    3,  118,
       8,    8, 0x0,    2,  124,
      11,   11, 0x0,    2,  128,
      13,   13, 0x0,    2,  132,
      17,   17, 0x0,    2,  136,

 // enum data: key, value
      25, uint(QListView::Static),
      26, uint(QListView::Free),
      27, uint(QListView::Snap),
      28, uint(QListView::LeftToRight),
      29, uint(QListView::TopToBottom),
      30, uint(QListView::Fixed),
      31, uint(QListView::Adjust),
      32, uint(QListView::SinglePass),
      33, uint(QListView::Batched),
      34, uint(QListView::ListMode),
      35, uint(QListView::IconMode),

       0        // eod
};

Q_CONSTINIT const QMetaObject QListView::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractItemView::staticMetaObject>(),
    qt_meta_stringdata_ZN9QListViewE.offsetsAndSizes,
    qt_meta_data_ZN9QListViewE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN9QListViewE_t,
        // property 'movement'
        QtPrivate::TypeAndForceComplete<Movement, std::true_type>,
        // property 'flow'
        QtPrivate::TypeAndForceComplete<Flow, std::true_type>,
        // property 'isWrapping'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'resizeMode'
        QtPrivate::TypeAndForceComplete<ResizeMode, std::true_type>,
        // property 'layoutMode'
        QtPrivate::TypeAndForceComplete<LayoutMode, std::true_type>,
        // property 'spacing'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'gridSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'viewMode'
        QtPrivate::TypeAndForceComplete<ViewMode, std::true_type>,
        // property 'modelColumn'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'uniformItemSizes'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'batchSize'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'wordWrap'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'selectionRectVisible'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'itemAlignment'
        QtPrivate::TypeAndForceComplete<Qt::Alignment, std::true_type>,
        // enum 'Movement'
        QtPrivate::TypeAndForceComplete<QListView::Movement, std::true_type>,
        // enum 'Flow'
        QtPrivate::TypeAndForceComplete<QListView::Flow, std::true_type>,
        // enum 'ResizeMode'
        QtPrivate::TypeAndForceComplete<QListView::ResizeMode, std::true_type>,
        // enum 'LayoutMode'
        QtPrivate::TypeAndForceComplete<QListView::LayoutMode, std::true_type>,
        // enum 'ViewMode'
        QtPrivate::TypeAndForceComplete<QListView::ViewMode, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QListView, std::true_type>,
        // method 'indexesMoved'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QModelIndexList &, std::false_type>
    >,
    nullptr
} };

void QListView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QListView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->indexesMoved((*reinterpret_cast< std::add_pointer_t<QModelIndexList>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QModelIndexList >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QListView::*)(const QModelIndexList & );
            if (_q_method_type _q_method = &QListView::indexesMoved; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< Movement*>(_v) = _t->movement(); break;
        case 1: *reinterpret_cast< Flow*>(_v) = _t->flow(); break;
        case 2: *reinterpret_cast< bool*>(_v) = _t->isWrapping(); break;
        case 3: *reinterpret_cast< ResizeMode*>(_v) = _t->resizeMode(); break;
        case 4: *reinterpret_cast< LayoutMode*>(_v) = _t->layoutMode(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->spacing(); break;
        case 6: *reinterpret_cast< QSize*>(_v) = _t->gridSize(); break;
        case 7: *reinterpret_cast< ViewMode*>(_v) = _t->viewMode(); break;
        case 8: *reinterpret_cast< int*>(_v) = _t->modelColumn(); break;
        case 9: *reinterpret_cast< bool*>(_v) = _t->uniformItemSizes(); break;
        case 10: *reinterpret_cast< int*>(_v) = _t->batchSize(); break;
        case 11: *reinterpret_cast< bool*>(_v) = _t->wordWrap(); break;
        case 12: *reinterpret_cast< bool*>(_v) = _t->isSelectionRectVisible(); break;
        case 13: *reinterpret_cast< Qt::Alignment*>(_v) = _t->itemAlignment(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setMovement(*reinterpret_cast< Movement*>(_v)); break;
        case 1: _t->setFlow(*reinterpret_cast< Flow*>(_v)); break;
        case 2: _t->setWrapping(*reinterpret_cast< bool*>(_v)); break;
        case 3: _t->setResizeMode(*reinterpret_cast< ResizeMode*>(_v)); break;
        case 4: _t->setLayoutMode(*reinterpret_cast< LayoutMode*>(_v)); break;
        case 5: _t->setSpacing(*reinterpret_cast< int*>(_v)); break;
        case 6: _t->setGridSize(*reinterpret_cast< QSize*>(_v)); break;
        case 7: _t->setViewMode(*reinterpret_cast< ViewMode*>(_v)); break;
        case 8: _t->setModelColumn(*reinterpret_cast< int*>(_v)); break;
        case 9: _t->setUniformItemSizes(*reinterpret_cast< bool*>(_v)); break;
        case 10: _t->setBatchSize(*reinterpret_cast< int*>(_v)); break;
        case 11: _t->setWordWrap(*reinterpret_cast< bool*>(_v)); break;
        case 12: _t->setSelectionRectVisible(*reinterpret_cast< bool*>(_v)); break;
        case 13: _t->setItemAlignment(*reinterpret_cast< Qt::Alignment*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QListView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QListView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN9QListViewE.stringdata0))
        return static_cast<void*>(this);
    return QAbstractItemView::qt_metacast(_clname);
}

int QListView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractItemView::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
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
void QListView::indexesMoved(const QModelIndexList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
