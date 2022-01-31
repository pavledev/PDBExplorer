#pragma once

#include <QObject>
#include "PDB.h"
#include "ProcessType.h"

class PDBProcess : public QObject
{
	Q_OBJECT

public:
	PDBProcess(QObject* parent, PDB* pdb, ProcessType processType, SymbolRecord* symbolRecord = nullptr);
	~PDBProcess();

	void Stop();

signals:
	void Completed();
	void SetProgressMinimum(int min);
	void SetProgressMaximum(int max);
	void SetProgressValue(int value);

	void SendStatusMessageToProcessDialog(const QString& statusMessage);

public slots:
	void Process();

private:
	PDB* pdb;
	ProcessType processType;
	SymbolRecord* symbolRecord;
};
