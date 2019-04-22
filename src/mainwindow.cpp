#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

static QList<QLineEdit*> rootEdits;

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui_(new Ui::MainWindow)
{
	// Initialize
	ui_->setupUi(this);
	ui_->settingsWidget->hide();
	addAction(ui_->actionSettings);
	rootEdits.append(QList<QLineEdit*>() << ui_->lineRoot0 << ui_->lineRoot1 << ui_->lineRoot2 << ui_->lineRoot3 << ui_->lineRoot4 << ui_->lineRoot5);

	// Connect signals and slots
	connect(ui_->fractalWidget, &FractalWidget::rootMoved, this, &MainWindow::on_rootMoved);
	connect(ui_->spinSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::on_settingsChanged);
	connect(ui_->spinIterations, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::on_settingsChanged);
	connect(ui_->spinDegree, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::on_settingsChanged);
	connect(ui_->btnReset, &QPushButton::clicked, ui_->fractalWidget, &FractalWidget::resetRoots);
	connect(ui_->actionSettings, &QAction::triggered, [this]() { ui_->settingsWidget->setVisible(ui_->settingsWidget->isHidden()); });
	for (QLineEdit *rootEdit : rootEdits) {
		connect(rootEdit, &QLineEdit::editingFinished, this, &MainWindow::on_settingsChanged);
	}

	// Start rendering with 3 default roots
	Parameters defaults(3);
	ui_->spinSize->setValue(defaults.resultSize.width());
	ui_->spinIterations->setValue(defaults.maxIterations);
	ui_->spinDegree->setValue(defaults.roots.count());
	ui_->fractalWidget->setParams(defaults);
	ui_->fractalWidget->resetRoots();
}

MainWindow::~MainWindow()
{
	// Clean
	delete ui_;
}

void MainWindow::on_settingsChanged()
{
	// Update fractal with new settings
	Parameters params;
	int wh = ui_->spinSize->value();
	quint8 degree = ui_->spinDegree->value();
	params.roots.clear();
	params.resultSize = QSize(wh, wh);
	params.maxIterations = ui_->spinIterations->value();

	// Update rootEdit visibility
	for (quint8 i = 0; i < N; ++i) {
		rootEdits[i]->setEnabled(i < degree);
		if (i < degree) {
			params.roots.append(string2complex(rootEdits[i]->text()));
		}
	}

	// Render
	ui_->fractalWidget->setParams(params);
}

void MainWindow::on_rootMoved(quint8 index, complex value)
{
	// Update settings
	if (index < N) {
		rootEdits[index]->setText(complex2string(value));
	}
}
