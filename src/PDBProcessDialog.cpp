#include "PDBProcessDialog.h"

PDBProcessDialog::PDBProcessDialog(QWidget* parent, PDB* pdb, ProcessType processType, SymbolRecord* symbolRecord) : QDialog(parent)
{
	ui.setupUi(this);

	if (processType == ProcessType::exportUDTsAndEnums)
	{
		setWindowTitle("Import");
	}
	else
	{
		setWindowTitle("Export");
	}

	if (processType != ProcessType::exportUDTsAndEnumsWithDependencies && processType != ProcessType::exportAllUDTsAndEnums)
	{
		ui.lblStatusMessage->setVisible(false);
	}

	thread = new QThread();

	pdbProcess = new PDBProcess(nullptr, pdb, processType, symbolRecord);
	pdbProcess->moveToThread(thread);

	connect(pdbProcess, &PDBProcess::Completed, this, &PDBProcessDialog::OnCompleted);
	connect(pdbProcess, &PDBProcess::SetProgressMinimum, this, &PDBProcessDialog::OnSetProgressMinimum);
	connect(pdbProcess, &PDBProcess::SetProgressMaximum, this, &PDBProcessDialog::OnSetProgressMaximum);
	connect(pdbProcess, &PDBProcess::SetProgressValue, this, &PDBProcessDialog::OnSetProgressValue);

	connect(pdbProcess, &PDBProcess::SendStatusMessageToProcessDialog, this, &PDBProcessDialog::DisplayStatusMessage);

	connect(thread, &QThread::started, pdbProcess, &PDBProcess::Process);

	thread->start();

	returnCode = QDialog::Accepted;
}

PDBProcessDialog::~PDBProcessDialog()
{
	pdbProcess->Stop();

	thread->quit();
	thread->wait();

	delete thread;
}

void PDBProcessDialog::BtnCancelClicked()
{
	returnCode = QDialog::Rejected;

	pdbProcess->Stop();
	done(returnCode);
}

void PDBProcessDialog::OnCompleted()
{
	done(returnCode);
}

void PDBProcessDialog::OnSetProgressMinimum(int min)
{
	ui.progressBar->setMinimum(min);
}

void PDBProcessDialog::OnSetProgressMaximum(int max)
{
	ui.progressBar->setMaximum(max);
}

void PDBProcessDialog::OnSetProgressValue(int value)
{
	ui.progressBar->setValue(value);
}

void PDBProcessDialog::DisplayStatusMessage(const QString& message)
{
	ui.lblStatusMessage->setText(message);
}
