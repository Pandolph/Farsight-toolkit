/*=========================================================================
Copyright 2009 Rensselaer Polytechnic Institute
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
=========================================================================*/
#ifndef TRAININGDIALOG_H
#define TRAININGDIALOG_H

//QT INCLUDES
#include <QtGui/QLabel>
#include <QtGui/QDialog>
#include <QtGui/QPushButton>
#include <QtGui/QRadioButton>
#include <QtGui/QButtonGroup>
#include <QtGui/QVBoxLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QCheckBox>
#include <QtGui/QScrollArea>
#include <QtGui/QMessageBox>
#include <QtGui/QLineEdit>

//VTK INCLUDES
#include <vtkTable.h>
#include <vtkSmartPointer.h>
#include <vtkDoubleArray.h>

#include <iostream>
#include <vector>
#include <set>
#include <list>
#include <float.h>

class TrainingDialog : public QDialog
{
    Q_OBJECT;

public:
  TrainingDialog(vtkSmartPointer<vtkTable> table, const char * trainColumn, QWidget *parent = 0);

public slots:
	  void accept();

protected:

signals:
	void changedTable(void);

private slots:
	void addClass(void);
	void remClass(void);
	void saveModel(void);
	void updateTable(void);
	void parseInputValues(void);

private:
	QVBoxLayout * inputsLayout;
	QVector<QHBoxLayout *> iLayouts;
	QVector<QLabel *> inputLabels;
	QVector<QLineEdit *> inputValues;
	QPushButton * addButton;
	QPushButton * delButton;
	QPushButton * saveButton;
	QPushButton * quitButton;
	QPushButton * doneButton;

	std::vector< std::set<int> > training;
	vtkSmartPointer<vtkTable> m_table;
	const char * columnForTraining;
};

#endif
