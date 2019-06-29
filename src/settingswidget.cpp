// This file is part of the NewtonFractal project.
// Copyright (C) 2019 Christian Bauer and Timon Foehl
// License: GNU General Public License version 3 or later,
// see the file LICENSE in the main directory.

#include "settingswidget.h"
#include "ui_settingswidget.h"
#include "fractalwidget.h"
#include "parameters.h"
#include "rootedit.h"
#include "rooticon.h"
#include <QDesktopServices>
#include <QStandardPaths>
#include <QColorDialog>
#include <QFileDialog>
#include <QSettings>
#include <QDateTime>
#include <QMenu>
#include <QUrl>

SettingsWidget::SettingsWidget(Parameters *params, QWidget *parent) :
	QWidget(parent),
	ui_(new Ui::SettingsWidget),
	params_(params)
{
	// Initialize ui
	ui_->setupUi(this);
	ui_->cbThreading->setEditable(true);
	ui_->cbThreading->lineEdit()->setReadOnly(true);
	ui_->cbThreading->lineEdit()->setAlignment(Qt::AlignCenter);
	ui_->lineSize->setValue(params_->size);
	ui_->spinScale->setValue(params_->scaleDownFactor * 100);
	ui_->spinIterations->setValue(params_->maxIterations);
	ui_->spinDegree->setValue(params_->roots.size());
	ui_->spinDamping->setValue(params_->damping);
	ui_->cbThreading->setCurrentIndex(params_->multiThreaded);
	ui_->spinZoom->setValue(params_->limits.zoomFactor() * 100);

	// Create actions for roots
	QAction *remove = new QAction(QIcon("://resources/icons/remove.png"), tr("Remove root"), this);
	QAction *changeColor = new QAction(QIcon("://resources/icons/color.png"), tr("Change color"), this);
	QAction *mirrorX = new QAction(QIcon("://resources/icons/mirrorx.png"), tr("Mirror on x-axis"), this);
	QAction *mirrorY = new QAction(QIcon("://resources/icons/mirrory.png"), tr("Mirror on y-axis"), this);
	rootActions_ << remove << changeColor << mirrorX << mirrorY;

	// Connect ui signals to slots
	connect(ui_->btnExportImage, &QPushButton::clicked, this, &SettingsWidget::on_btnExportImageClicked);
	connect(ui_->btnExportRoots, &QPushButton::clicked, this, &SettingsWidget::on_btnExportRootsClicked);
	connect(ui_->btnImportRoots, &QPushButton::clicked, this, &SettingsWidget::on_btnImportRootsClicked);
	connect(ui_->lineSize, &SizeEdit::sizeChanged, this, &SettingsWidget::sizeChanged);
	connect(ui_->btnReset, &QPushButton::clicked, this, &SettingsWidget::reset);
	connect(ui_->spinScale, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsWidget::on_settingsChanged);
	connect(ui_->spinIterations, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsWidget::on_settingsChanged);
	connect(ui_->spinDegree, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsWidget::on_settingsChanged);
	connect(ui_->spinDamping, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SettingsWidget::on_settingsChanged);
	connect(ui_->spinZoom, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SettingsWidget::on_settingsChanged);
	connect(ui_->cbThreading, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsWidget::on_settingsChanged);

	// Connect external links
	connect(ui_->btnOpit7, &QPushButton::clicked, [this]() {QDesktopServices::openUrl(QUrl("https://github.com/opit7"));});
	connect(ui_->btnChrizbee, &QPushButton::clicked, [this]() {QDesktopServices::openUrl(QUrl("https://github.com/chrizbee"));});
	connect(ui_->btnOhm, &QPushButton::clicked, [this]() {QDesktopServices::openUrl(QUrl("https://www.th-nuernberg.de/fakultaeten/efi"));});
	connect(ui_->btnIcons8, &QPushButton::clicked, [this]() {QDesktopServices::openUrl(QUrl("https://icons8.com"));});

	// Initialize roots
	ui_->spinDegree->setValue(DRC);
}

SettingsWidget::~SettingsWidget()
{
	// Clean
	delete ui_;
}

void SettingsWidget::toggle()
{
	// Toggle visibility
	setVisible(isHidden());
}

void SettingsWidget::changeSize(QSize size)
{
	// Change sizeedit
	ui_->lineSize->setValue(size);
}

void SettingsWidget::changeZoom(double factor)
{
	// Update settings
	ui_->spinZoom->setValue(factor * 100);
}

void SettingsWidget::addRoot(complex value)
{
	// Create new root and apend it to parameters
	Root root(value);
	int rootCount = params_->roots.size();
	if (params_->roots.size() < 6) root.setColor(colors[rootCount]);
	params_->roots.append(root);

	// Create RootEdit, add it to list and layout and connect its signal
	RootEdit *edit = new RootEdit(root.value(), this);
	rootEdits_.append(edit);
	ui_->gridRoots->addWidget(edit, rootCount + 1, 1);
	connect(edit, &RootEdit::rootChanged, this, &SettingsWidget::on_settingsChanged);

	// Create label icon
	RootIcon *icon = new RootIcon(root.color(), this);
	rootIcons_.append(icon);
	ui_->gridRoots->addWidget(icon, rootCount + 1, 0);
	connect(icon, &RootIcon::clicked, this, &SettingsWidget::openRootContextMenu);

	// Update spinbox
	ui_->spinDegree->setValue(params_->roots.size());
}

void SettingsWidget::removeRoot(qint8 index)
{
	// Remove rootedit and icon
	quint8 rootCount = params_->roots.size();
	index = index < 0 ? rootCount - 1 : index;
	RootEdit *edit = rootEdits_.takeAt(index);
	RootIcon *icon = rootIcons_.takeAt(index);
	ui_->gridRoots->removeWidget(edit);
	ui_->gridRoots->removeWidget(icon);
	params_->roots.removeAt(index);
	delete edit;
	delete icon;

	// Shift others up
	for (quint8 i = index; i < rootCount - 1; ++i) {
		ui_->gridRoots->removeWidget(rootEdits_[i]);
		ui_->gridRoots->removeWidget(rootIcons_[i]);
		ui_->gridRoots->addWidget(rootEdits_[i], i + 1, 1);
		ui_->gridRoots->addWidget(rootIcons_[i], i + 1, 0);
	}

	// Update spinbox
	ui_->spinDegree->setValue(params_->roots.size());
}

void SettingsWidget::moveRoot(quint8 index, complex value)
{
	// Update settings
	if (index < rootEdits_.size()) {
		rootEdits_[index]->setValue(value);
	}
}

void SettingsWidget::openRootContextMenu()
{
	// Open root context menu
	RootIcon *icon = static_cast<RootIcon*>(sender());
	if (icon != nullptr) {
		quint8 index = rootIcons_.indexOf(icon);
		QAction *clicked = QMenu::exec(rootActions_, icon->mapToGlobal(QPoint(0, 0)), rootActions_.first(), this);

		// Remove root
		if (clicked == rootActions_[0]) {
			removeRoot(index);

		// Open color dialog
		} else if (clicked == rootActions_[1]) {
			QColor color = QColorDialog::getColor(params_->roots[index].color(), this);
			if (color.isValid()) {
				icon->setColor(color);
				params_->roots[index].setColor(color);
			}

		// Mirror root on x-axis
		} else if (clicked == rootActions_[2]) {
			complex root = params_->roots[index].value();
			root.imag(-root.imag());
			addRoot(root);

		// Mirror root on y-axis
		} else if (clicked == rootActions_[3]) {
			complex root = params_->roots[index].value();
			root.real(-root.real());
			addRoot(root);
		}
	}

	// Re-render
	emit paramsChanged();
}

void SettingsWidget::on_settingsChanged()
{
	// Add / remove roots
	int degree = ui_->spinDegree->value();
	while (params_->roots.size() < degree) { addRoot(); }
	while (params_->roots.size() > degree) { removeRoot(); }

	// Update fractal with new settings
	params_->limits.setZoomFactor(ui_->spinZoom->value() / 100.0);
	params_->maxIterations = ui_->spinIterations->value();
	params_->damping = ui_->spinDamping->value();
	params_->scaleDownFactor = ui_->spinScale->value() / 100.0;
	params_->multiThreaded = ui_->cbThreading->currentIndex();

	// Update rootEdit value
	if (rootEdits_.size() == params_->roots.size()) {
		for (quint8 i = 0; i < rootEdits_.size(); ++i) {
			params_->roots[i].setValue(rootEdits_[i]->value());
		}
	}

	// Re-render
	emit paramsChanged();
}

void SettingsWidget::on_btnExportImageClicked()
{
	// Export pixmap to file
	QSettings settings;
	QString dir = settings.value("imagedir", QStandardPaths::standardLocations(QStandardPaths::PicturesLocation)).toString();
	dir = QFileDialog::getExistingDirectory(this, tr("Export fractal to"), dir);
	if (!dir.isEmpty()) {
		settings.setValue("imagedir", dir);
		emit exportImage(dir);
	}
}

void SettingsWidget::on_btnExportRootsClicked()
{
	// Export roots to file
	QSettings settings;
	QString dir = settings.value("rootsdir", QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)).toString();
	dir = QFileDialog::getExistingDirectory(this, tr("Export roots to"), dir);
	if (!dir.isEmpty()) {
		settings.setValue("rootsdir", dir);
		emit exportRoots(dir);
	}
}

void SettingsWidget::on_btnImportRootsClicked()
{
	// Import roots from file
	QSettings settings;
	QString dir = settings.value("rootsdir", QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)).toString();
	QString file = QFileDialog::getOpenFileName(this, tr("Import roots"), dir, tr("Roots-File (*.roots)"));
	if (!file.isEmpty()) {
		settings.setValue("rootsdir", dir);
		emit importRoots(file);
	}
}
