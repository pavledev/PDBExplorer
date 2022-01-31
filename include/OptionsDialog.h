#pragma once

#include <QDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QFileDialog>
#include "ui_OptionsDialog.h"
#include "Options.h"

class OptionsDialog : public QDialog
{
	Q_OBJECT

public:
	OptionsDialog(QWidget* parent, Options* options, bool* optionsChanged);
	~OptionsDialog();

	static void LoadOptions(Options* options);

private:
	Ui::OptionsDialog ui;
	Options* options;
	bool* optionsChanged;

	void SaveOptions();
	void CheckIfOptionsChanged();

private slots:
	void BtnOkClicked();
	void BtnCancelClicked();

	void ChkModifyFunctionNamesClicked();
	void ChkModifyVariableNamesClicked();
};
