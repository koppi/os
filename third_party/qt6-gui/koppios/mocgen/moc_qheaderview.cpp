/****************************************************************************
** Meta object code from reading C++ file 'qheaderview.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/itemviews/qheaderview.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qheaderview.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11QHeaderViewE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN11QHeaderViewE = QtMocHelpers::stringData(
    "QHeaderView",
    "sectionMoved",
    "",
    "logicalIndex",
    "oldVisualIndex",
    "newVisualIndex",
    "sectionResized",
    "oldSize",
    "newSize",
    "sectionPressed",
    "sectionClicked",
    "sectionEntered",
    "sectionDoubleClicked",
    "sectionCountChanged",
    "oldCount",
    "newCount",
    "sectionHandleDoubleClicked",
    "geometriesChanged",
    "sortIndicatorChanged",
    "Qt::SortOrder",
    "order",
    "sortIndicatorClearableChanged",
    "clearable",
    "setOffset",
    "offset",
    "setOffsetToSectionPosition",
    "visualIndex",
    "setOffsetToLastSection",
    "headerDataChanged",
    "Qt::Orientation",
    "orientation",
    "logicalFirst",
    "logicalLast",
    "updateSection",
    "resizeSections",
    "sectionsInserted",
    "QModelIndex",
    "parent",
    "sectionsAboutToBeRemoved",
    "firstSectionMovable",
    "showSortIndicator",
    "sectionsMovable",
    "sectionsClickable",
    "highlightSections",
    "stretchLastSection",
    "cascadingSectionResizes",
    "defaultSectionSize",
    "minimumSectionSize",
    "maximumSectionSize",
    "defaultAlignment",
    "Qt::Alignment",
    "sortIndicatorClearable",
    "ResizeMode",
    "Interactive",
    "Stretch",
    "Fixed",
    "ResizeToContents",
    "Custom"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN11QHeaderViewE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      19,   14, // methods
      12,  203, // properties
       1,  263, // enums/sets
       0,    0, // constructors
       0,       // flags
      11,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    3,  128,    2, 0x06,   14 /* Public */,
       6,    3,  135,    2, 0x06,   18 /* Public */,
       9,    1,  142,    2, 0x06,   22 /* Public */,
      10,    1,  145,    2, 0x06,   24 /* Public */,
      11,    1,  148,    2, 0x06,   26 /* Public */,
      12,    1,  151,    2, 0x06,   28 /* Public */,
      13,    2,  154,    2, 0x06,   30 /* Public */,
      16,    1,  159,    2, 0x06,   33 /* Public */,
      17,    0,  162,    2, 0x06,   35 /* Public */,
      18,    2,  163,    2, 0x06,   36 /* Public */,
      21,    1,  168,    2, 0x06,   39 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      23,    1,  171,    2, 0x0a,   41 /* Public */,
      25,    1,  174,    2, 0x0a,   43 /* Public */,
      27,    0,  177,    2, 0x0a,   45 /* Public */,
      28,    3,  178,    2, 0x0a,   46 /* Public */,
      33,    1,  185,    2, 0x09,   50 /* Protected */,
      34,    0,  188,    2, 0x09,   52 /* Protected */,
      35,    3,  189,    2, 0x09,   53 /* Protected */,
      38,    3,  196,    2, 0x09,   57 /* Protected */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int,    3,    4,    5,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int,    3,    7,    8,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   14,   15,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 19,    3,   20,
    QMetaType::Void, QMetaType::Bool,   22,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,   24,
    QMetaType::Void, QMetaType::Int,   26,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 29, QMetaType::Int, QMetaType::Int,   30,   31,   32,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 36, QMetaType::Int, QMetaType::Int,   37,   31,   32,
    QMetaType::Void, 0x80000000 | 36, QMetaType::Int, QMetaType::Int,   37,   31,   32,

 // properties: name, type, flags, notifyId, revision
      39, QMetaType::Bool, 0x00015103, uint(-1), 0,
      40, QMetaType::Bool, 0x00015003, uint(-1), 0,
      41, QMetaType::Bool, 0x00015103, uint(-1), 0,
      42, QMetaType::Bool, 0x00015103, uint(-1), 0,
      43, QMetaType::Bool, 0x00015103, uint(-1), 0,
      44, QMetaType::Bool, 0x00015103, uint(-1), 0,
      45, QMetaType::Bool, 0x00015103, uint(-1), 0,
      46, QMetaType::Int, 0x00015107, uint(-1), 0,
      47, QMetaType::Int, 0x00015103, uint(-1), 0,
      48, QMetaType::Int, 0x00015103, uint(-1), 0,
      49, 0x80000000 | 50, 0x0001510b, uint(-1), 0,
      51, QMetaType::Bool, 0x00015103, uint(10), 0,

 // enums: name, alias, flags, count, data
      52,   52, 0x0,    5,  268,

 // enum data: key, value
      53, uint(QHeaderView::Interactive),
      54, uint(QHeaderView::Stretch),
      55, uint(QHeaderView::Fixed),
      56, uint(QHeaderView::ResizeToContents),
      57, uint(QHeaderView::Custom),

       0        // eod
};

Q_CONSTINIT const QMetaObject QHeaderView::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractItemView::staticMetaObject>(),
    qt_meta_stringdata_ZN11QHeaderViewE.offsetsAndSizes,
    qt_meta_data_ZN11QHeaderViewE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN11QHeaderViewE_t,
        // property 'firstSectionMovable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'showSortIndicator'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'sectionsMovable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'sectionsClickable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'highlightSections'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'stretchLastSection'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'cascadingSectionResizes'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'defaultSectionSize'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'minimumSectionSize'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maximumSectionSize'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'defaultAlignment'
        QtPrivate::TypeAndForceComplete<Qt::Alignment, std::true_type>,
        // property 'sortIndicatorClearable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // enum 'ResizeMode'
        QtPrivate::TypeAndForceComplete<QHeaderView::ResizeMode, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QHeaderView, std::true_type>,
        // method 'sectionMoved'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'sectionResized'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'sectionPressed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'sectionClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'sectionEntered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'sectionDoubleClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'sectionCountChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'sectionHandleDoubleClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'geometriesChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'sortIndicatorChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::SortOrder, std::false_type>,
        // method 'sortIndicatorClearableChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setOffset'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setOffsetToSectionPosition'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setOffsetToLastSection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'headerDataChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::Orientation, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'updateSection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'resizeSections'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'sectionsInserted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QModelIndex &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'sectionsAboutToBeRemoved'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QModelIndex &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>
    >,
    nullptr
} };

void QHeaderView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QHeaderView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->sectionMoved((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 1: _t->sectionResized((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 2: _t->sectionPressed((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->sectionClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->sectionEntered((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->sectionDoubleClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->sectionCountChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 7: _t->sectionHandleDoubleClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->geometriesChanged(); break;
        case 9: _t->sortIndicatorChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Qt::SortOrder>>(_a[2]))); break;
        case 10: _t->sortIndicatorClearableChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 11: _t->setOffset((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 12: _t->setOffsetToSectionPosition((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 13: _t->setOffsetToLastSection(); break;
        case 14: _t->headerDataChanged((*reinterpret_cast< std::add_pointer_t<Qt::Orientation>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 15: _t->updateSection((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 16: _t->resizeSections(); break;
        case 17: _t->sectionsInserted((*reinterpret_cast< std::add_pointer_t<QModelIndex>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 18: _t->sectionsAboutToBeRemoved((*reinterpret_cast< std::add_pointer_t<QModelIndex>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QHeaderView::*)(int , int , int );
            if (_q_method_type _q_method = &QHeaderView::sectionMoved; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QHeaderView::*)(int , int , int );
            if (_q_method_type _q_method = &QHeaderView::sectionResized; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QHeaderView::*)(int );
            if (_q_method_type _q_method = &QHeaderView::sectionPressed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QHeaderView::*)(int );
            if (_q_method_type _q_method = &QHeaderView::sectionClicked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QHeaderView::*)(int );
            if (_q_method_type _q_method = &QHeaderView::sectionEntered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (QHeaderView::*)(int );
            if (_q_method_type _q_method = &QHeaderView::sectionDoubleClicked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (QHeaderView::*)(int , int );
            if (_q_method_type _q_method = &QHeaderView::sectionCountChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (QHeaderView::*)(int );
            if (_q_method_type _q_method = &QHeaderView::sectionHandleDoubleClicked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (QHeaderView::*)();
            if (_q_method_type _q_method = &QHeaderView::geometriesChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (QHeaderView::*)(int , Qt::SortOrder );
            if (_q_method_type _q_method = &QHeaderView::sortIndicatorChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (QHeaderView::*)(bool );
            if (_q_method_type _q_method = &QHeaderView::sortIndicatorClearableChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isFirstSectionMovable(); break;
        case 1: *reinterpret_cast< bool*>(_v) = _t->isSortIndicatorShown(); break;
        case 2: *reinterpret_cast< bool*>(_v) = _t->sectionsMovable(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->sectionsClickable(); break;
        case 4: *reinterpret_cast< bool*>(_v) = _t->highlightSections(); break;
        case 5: *reinterpret_cast< bool*>(_v) = _t->stretchLastSection(); break;
        case 6: *reinterpret_cast< bool*>(_v) = _t->cascadingSectionResizes(); break;
        case 7: *reinterpret_cast< int*>(_v) = _t->defaultSectionSize(); break;
        case 8: *reinterpret_cast< int*>(_v) = _t->minimumSectionSize(); break;
        case 9: *reinterpret_cast< int*>(_v) = _t->maximumSectionSize(); break;
        case 10: *reinterpret_cast< Qt::Alignment*>(_v) = _t->defaultAlignment(); break;
        case 11: *reinterpret_cast< bool*>(_v) = _t->isSortIndicatorClearable(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setFirstSectionMovable(*reinterpret_cast< bool*>(_v)); break;
        case 1: _t->setSortIndicatorShown(*reinterpret_cast< bool*>(_v)); break;
        case 2: _t->setSectionsMovable(*reinterpret_cast< bool*>(_v)); break;
        case 3: _t->setSectionsClickable(*reinterpret_cast< bool*>(_v)); break;
        case 4: _t->setHighlightSections(*reinterpret_cast< bool*>(_v)); break;
        case 5: _t->setStretchLastSection(*reinterpret_cast< bool*>(_v)); break;
        case 6: _t->setCascadingSectionResizes(*reinterpret_cast< bool*>(_v)); break;
        case 7: _t->setDefaultSectionSize(*reinterpret_cast< int*>(_v)); break;
        case 8: _t->setMinimumSectionSize(*reinterpret_cast< int*>(_v)); break;
        case 9: _t->setMaximumSectionSize(*reinterpret_cast< int*>(_v)); break;
        case 10: _t->setDefaultAlignment(*reinterpret_cast< Qt::Alignment*>(_v)); break;
        case 11: _t->setSortIndicatorClearable(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
if (_c == QMetaObject::ResetProperty) {
        switch (_id) {
        case 7: _t->resetDefaultSectionSize(); break;
        default: break;
        }
    }
}

const QMetaObject *QHeaderView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QHeaderView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN11QHeaderViewE.stringdata0))
        return static_cast<void*>(this);
    return QAbstractItemView::qt_metacast(_clname);
}

int QHeaderView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractItemView::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 19)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 19)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 19;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void QHeaderView::sectionMoved(int _t1, int _t2, int _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QHeaderView::sectionResized(int _t1, int _t2, int _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QHeaderView::sectionPressed(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QHeaderView::sectionClicked(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void QHeaderView::sectionEntered(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void QHeaderView::sectionDoubleClicked(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void QHeaderView::sectionCountChanged(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void QHeaderView::sectionHandleDoubleClicked(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void QHeaderView::geometriesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void QHeaderView::sortIndicatorChanged(int _t1, Qt::SortOrder _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}

// SIGNAL 10
void QHeaderView::sortIndicatorClearableChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 10, _a);
}
QT_WARNING_POP
