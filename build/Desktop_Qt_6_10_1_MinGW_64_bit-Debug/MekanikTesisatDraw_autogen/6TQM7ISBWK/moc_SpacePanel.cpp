/****************************************************************************
** Meta object code from reading C++ file 'SpacePanel.hpp'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../vkt/ui/SpacePanel.hpp"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SpacePanel.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.1. It"
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
struct qt_meta_tag_ZN3vkt2ui10SpacePanelE_t {};
} // unnamed namespace

template <> constexpr inline auto vkt::ui::SpacePanel::qt_create_metaobjectdata<qt_meta_tag_ZN3vkt2ui10SpacePanelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "vkt::ui::SpacePanel",
        "SpaceSelected",
        "",
        "spaceId",
        "SpaceDoubleClicked",
        "SpacePropertiesChanged",
        "DeleteSpaceRequested",
        "OnSpaceSelected",
        "OnSpaceDoubleClicked",
        "QTreeWidgetItem*",
        "item",
        "column",
        "OnFilterChanged",
        "text",
        "OnTypeFilterChanged",
        "index",
        "OnDeleteSpace",
        "OnRefresh",
        "OnShowStatistics"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'SpaceSelected'
        QtMocHelpers::SignalData<void(unsigned long long)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 3 },
        }}),
        // Signal 'SpaceDoubleClicked'
        QtMocHelpers::SignalData<void(unsigned long long)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 3 },
        }}),
        // Signal 'SpacePropertiesChanged'
        QtMocHelpers::SignalData<void(unsigned long long)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 3 },
        }}),
        // Signal 'DeleteSpaceRequested'
        QtMocHelpers::SignalData<void(unsigned long long)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 3 },
        }}),
        // Slot 'OnSpaceSelected'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnSpaceDoubleClicked'
        QtMocHelpers::SlotData<void(QTreeWidgetItem *, int)>(8, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 9, 10 }, { QMetaType::Int, 11 },
        }}),
        // Slot 'OnFilterChanged'
        QtMocHelpers::SlotData<void(const QString &)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Slot 'OnTypeFilterChanged'
        QtMocHelpers::SlotData<void(int)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 15 },
        }}),
        // Slot 'OnDeleteSpace'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnRefresh'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnShowStatistics'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SpacePanel, qt_meta_tag_ZN3vkt2ui10SpacePanelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject vkt::ui::SpacePanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QDockWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3vkt2ui10SpacePanelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3vkt2ui10SpacePanelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN3vkt2ui10SpacePanelE_t>.metaTypes,
    nullptr
} };

void vkt::ui::SpacePanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SpacePanel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->SpaceSelected((*reinterpret_cast<std::add_pointer_t<qulonglong>>(_a[1]))); break;
        case 1: _t->SpaceDoubleClicked((*reinterpret_cast<std::add_pointer_t<qulonglong>>(_a[1]))); break;
        case 2: _t->SpacePropertiesChanged((*reinterpret_cast<std::add_pointer_t<qulonglong>>(_a[1]))); break;
        case 3: _t->DeleteSpaceRequested((*reinterpret_cast<std::add_pointer_t<qulonglong>>(_a[1]))); break;
        case 4: _t->OnSpaceSelected(); break;
        case 5: _t->OnSpaceDoubleClicked((*reinterpret_cast<std::add_pointer_t<QTreeWidgetItem*>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 6: _t->OnFilterChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->OnTypeFilterChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->OnDeleteSpace(); break;
        case 9: _t->OnRefresh(); break;
        case 10: _t->OnShowStatistics(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SpacePanel::*)(unsigned long long )>(_a, &SpacePanel::SpaceSelected, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpacePanel::*)(unsigned long long )>(_a, &SpacePanel::SpaceDoubleClicked, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpacePanel::*)(unsigned long long )>(_a, &SpacePanel::SpacePropertiesChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpacePanel::*)(unsigned long long )>(_a, &SpacePanel::DeleteSpaceRequested, 3))
            return;
    }
}

const QMetaObject *vkt::ui::SpacePanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *vkt::ui::SpacePanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3vkt2ui10SpacePanelE_t>.strings))
        return static_cast<void*>(this);
    return QDockWidget::qt_metacast(_clname);
}

int vkt::ui::SpacePanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDockWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void vkt::ui::SpacePanel::SpaceSelected(unsigned long long _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void vkt::ui::SpacePanel::SpaceDoubleClicked(unsigned long long _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void vkt::ui::SpacePanel::SpacePropertiesChanged(unsigned long long _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void vkt::ui::SpacePanel::DeleteSpaceRequested(unsigned long long _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}
QT_WARNING_POP
