#pragma once

#include <QDialog>
#include <QThread>
#include <QStatusBar>
#include "ui_PDBProcessDialog.h"
#include "PDBProcess.h"

class PDBProcessDialog : public QDialog
{
	Q_OBJECT

public:
	PDBProcessDialog(QWidget* parent, PDB* pdb, ProcessType processType, SymbolRecord* symbolRecord = nullptr);
	~PDBProcessDialog();

public slots:
	void OnCompleted();
	void OnSetProgressMinimum(int min);
	void OnSetProgressMaximum(int max);
	void OnSetProgressValue(int value);

	void DisplayStatusMessage(const QString& message);

private:
	Ui::PDBProcessDialog ui;
	QThread* thread;
	PDBProcess* pdbProcess;
	int returnCode;

private slots:
	void BtnCancelClicked();
};
