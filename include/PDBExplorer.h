#pragma once

#include <unordered_map>
#include <vector>
#include "DIA SDK/dia2.h"
#include <QtWidgets/QMainWindow>
#include <QFileDialog>
#include <QSettings>
#include <QStandardpaths>
#include <QFileinfo>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QMimeData>
#include <QHBoxLayout>
#include <QStringListModel>
#include <QTextStream>
#include <QDirIterator>
#include <QProcess>
#include <QTextEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QStackedLayout>
#include <QLibrary>
#include "lexilla/Lexilla.h"
#include "lexilla/SciLexer.h"
#include "scintilla/ScintillaEdit.h"
#include "PEHeaderParser.h"
#include "PDB.h"
#include "PDBProcessDialog.h"
#include "OptionsDialog.h"
#include "Options.h"
#include "ui_PDBExplorer.h"

class PDBExplorer : public QMainWindow
{
    Q_OBJECT

public:
    PDBExplorer(QWidget *parent = Q_NULLPTR);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    Ui::PDBExplorerClass ui;
    QSortFilterProxyModel* symbolsViewProxyModel;
    QSortFilterProxyModel* proxyModel;
    QSortFilterProxyModel* proxyModel2;
    QStringListModel* model;
    PEHeaderParser* peHeaderParser;
    PDB* pdb;
    QHash<QString, DWORD> diaSymbols;
    std::vector<SymbolRecord> symbolRecords;
    Options options;
    std::unordered_map<std::string, std::string> dllImports;
    CV_CPU_TYPE_e machineType;
    ScintillaEdit* codeEditor;
    ScintillaEdit* assemblyEditor;
    ScintillaEdit* assemblyEditor2;
    ScintillaEdit* assemblyEditor3;
    ScintillaEdit* pseudoCodeEditor;
    ScintillaEdit* pseudoCodeEditor2;
    QMenu* menu;
    QAction* action;
    QHash<QString, DWORD> variables;
    QHash<QString, DWORD> functions;
    QHash<QString, DWORD> publicSymbols;
    Element element;
    QLineEdit* txtFindItem;
    QLineEdit* txtMangledName;
    QLineEdit* txtDemangledName;
    QLineEdit* txtMangledName2;
    QLineEdit* txtDemangledName2;
    QLineEdit* txtAddress;
    QLineEdit* txtAddress2;
    QLineEdit* txtVirtualAddress;
    QLineEdit* txtRelativeVirtualAddress;
    QLineEdit* txtFileOffset;
    QPlainTextEdit* plainTextEdit;
    QTableView* tableView;
    QTableView* tvVTables;
    QComboBox* cbAddressTypes;
    QComboBox* cbAddressTypes2;
    QStackedLayout* stackedLayout;
    bool isFileOpened;
    QString filePath;
    MSVCDemangler msvcDemangler;

    void OpenFile(const QString& filePath);
    ProcessType GetProcessType();
    void DisplayFileInfo(const QString& filePath);
    void AddSymbolsToList();
    void AddSymbolsToList(SymbolType symbolType);
    void AddDataSymbolsToList(QHash<QString, DWORD>* variables);
    void AddDataSymbolsToList(QHash<QString, DWORD>* variables, bool isGlobal);
    void AddFunctionSymbolsToList(QHash<QString, DWORD>* functions);
    void AddFunctionSymbolsToList(QHash<QString, DWORD>* functions, bool isGlobal, bool isStatic, bool isMember);
    void AddPublicSymbolsToList();
    void RemoveSymbolsFromList(SymbolType symbolType);
    void RemoveSymbolsFromList(const int type);
    void HandleTableViewEvent();
    SymbolRecord GetSelectedSymbolRecord();
    void HandleUDTAndEnumType();
    void HandleDataType();
    void HandleFunctionType();
    void HandlePublicSymbolType();
    void EnableUDTAndEnumOptions();
    void DisableUDTAndEnumOptions();

    void SetupCodeEdtor(ScintillaEdit* codeEditor);
    void SetupAssemblyEditor(ScintillaEdit* assemblyEditor);
    void SetupLayouts();

    void DisplayHeaderCode();
    void DisplayCPPCode();
    void DisplayVariableInfo();
    void DisplayFunctionInfo();
    void DisplayPublicSymbolInfo();
    void DisplayStructureView();
    void DisplayFunctionOffsets();
    void GetVTables(const Element* element, QStringList* vTables);
    void DisplayVTables();
    void DisplayVTable(int vTableNum);
    void DisplayUDTLayout();
    void DisplayVTablesLayout();
    void DisplayMSVCLayout();
    void DisplayModulesInfo();
    void DisplayLinesInfo();
    void AddItemToModel(const QString& name, const QString& type, DWORD offset, DWORD size, QStandardItemModel* model, int row);
    void AddItemToModel(const QString& name, DWORD virtualOffset, DWORD fileOffset, QStandardItemModel* model, int row);
    void AddItemToModel(const QString& functionPrototype, DWORD vTableIndex, QStandardItemModel* model, int row);
    void AddFunctionsToModel(const Element* element, QStandardItemModel* model, int* row, const QString& currentUDTName);
    void SearchList();
    QString ConvertBOOLToString(BOOL state);
    QString ConvertAddressToString(DWORD address);

private slots:
    void OpenActionTriggered();
    void ExitActionTriggered();
    void OptionsActionTriggered();
    void ExportAllTypesActionTriggered();
    void ClearCacheActionTriggered();

    void TxtSearchSymbolTextChanged(const QString& text);
    void BtnSearchSymbolClicked();
    void TVSymbolsClicked(const QModelIndex& index);
    void TxtFindItemTextChanged(const QString& text);
    void LVVTablesClicked(const QModelIndex& index);
    void TxtMangledNameTextChanged(const QString& text);
    void TxtMangledName2TextChanged(const QString& text);
    void TxtVirtualAddressTextChanged(const QString& text);
    void TxtRelativeVirtualAddressTextChanged(const QString& text);
    void TxtFileOffsetTextChanged(const QString& text);

    void ChkClassesToggled();
    void ChkStructsToggled();
    void ChkUnionsToggled();
    void ChkEnumsToggled();
    void ChkGlobalToggled();
    void ChkStaticToggled();
    void ChkMemberToggled();

    void CbSymbolTypesCurrentIndexChanged(int index);
    void CbDisplayOptionsCurrentIndexChanged(int index);

    void DisplayStatusMessage(const QString& message);
    void ExportSymbol();
    void CustomMenuRequested(QPoint position);
};
