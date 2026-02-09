/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.hpp'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../vkt/ui/MainWindow.hpp"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.hpp' doesn't include <QObject>."
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
struct qt_meta_tag_ZN3vkt2ui10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto vkt::ui::MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN3vkt2ui10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "vkt::ui::MainWindow",
        "OnNew",
        "",
        "OnOpen",
        "OnSave",
        "OnSaveAs",
        "OnImportDXF",
        "OnExit",
        "OnUndo",
        "OnRedo",
        "OnDelete",
        "OnDrawPipe",
        "OnDrawFixture",
        "OnDrawJunction",
        "OnSelectMode",
        "OnPlanView",
        "OnIsometricView",
        "OnZoomExtents",
        "OnRunHydraulics",
        "OnGenerateSchedule",
        "OnExportReport",
        "OnSelectSpace",
        "OnCalculateLoads",
        "OnSpaceSelected",
        "spaceId",
        "OnSpaceDoubleClicked",
        "OnDeleteSpace",
        "OnPropertiesUpdated",
        "OnDiameterChanged",
        "text",
        "OnMaterialChanged",
        "OnZetaChanged",
        "OnSlopeChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'OnNew'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnOpen'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnSave'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnSaveAs'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnImportDXF'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnExit'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnUndo'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnRedo'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnDelete'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnDrawPipe'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnDrawFixture'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnDrawJunction'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnSelectMode'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnPlanView'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnIsometricView'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnZoomExtents'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnRunHydraulics'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnGenerateSchedule'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnExportReport'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnSelectSpace'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnCalculateLoads'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnSpaceSelected'
        QtMocHelpers::SlotData<void(unsigned long long)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::ULongLong, 24 },
        }}),
        // Slot 'OnSpaceDoubleClicked'
        QtMocHelpers::SlotData<void(unsigned long long)>(25, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::ULongLong, 24 },
        }}),
        // Slot 'OnDeleteSpace'
        QtMocHelpers::SlotData<void(unsigned long long)>(26, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::ULongLong, 24 },
        }}),
        // Slot 'OnPropertiesUpdated'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'OnDiameterChanged'
        QtMocHelpers::SlotData<void(const QString &)>(28, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 29 },
        }}),
        // Slot 'OnMaterialChanged'
        QtMocHelpers::SlotData<void(const QString &)>(30, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 29 },
        }}),
        // Slot 'OnZetaChanged'
        QtMocHelpers::SlotData<void(const QString &)>(31, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 29 },
        }}),
        // Slot 'OnSlopeChanged'
        QtMocHelpers::SlotData<void(const QString &)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 29 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN3vkt2ui10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject vkt::ui::MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3vkt2ui10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3vkt2ui10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN3vkt2ui10MainWindowE_t>.metaTypes,
    nullptr
} };

void vkt::ui::MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->OnNew(); break;
        case 1: _t->OnOpen(); break;
        case 2: _t->OnSave(); break;
        case 3: _t->OnSaveAs(); break;
        case 4: _t->OnImportDXF(); break;
        case 5: _t->OnExit(); break;
        case 6: _t->OnUndo(); break;
        case 7: _t->OnRedo(); break;
        case 8: _t->OnDelete(); break;
        case 9: _t->OnDrawPipe(); break;
        case 10: _t->OnDrawFixture(); break;
        case 11: _t->OnDrawJunction(); break;
        case 12: _t->OnSelectMode(); break;
        case 13: _t->OnPlanView(); break;
        case 14: _t->OnIsometricView(); break;
        case 15: _t->OnZoomExtents(); break;
        case 16: _t->OnRunHydraulics(); break;
        case 17: _t->OnGenerateSchedule(); break;
        case 18: _t->OnExportReport(); break;
        case 19: _t->OnSelectSpace(); break;
        case 20: _t->OnCalculateLoads(); break;
        case 21: _t->OnSpaceSelected((*reinterpret_cast<std::add_pointer_t<qulonglong>>(_a[1]))); break;
        case 22: _t->OnSpaceDoubleClicked((*reinterpret_cast<std::add_pointer_t<qulonglong>>(_a[1]))); break;
        case 23: _t->OnDeleteSpace((*reinterpret_cast<std::add_pointer_t<qulonglong>>(_a[1]))); break;
        case 24: _t->OnPropertiesUpdated(); break;
        case 25: _t->OnDiameterChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 26: _t->OnMaterialChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 27: _t->OnZetaChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 28: _t->OnSlopeChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *vkt::ui::MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *vkt::ui::MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3vkt2ui10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int vkt::ui::MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 29)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 29;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 29)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 29;
    }
    return _id;
}
QT_WARNING_POP
