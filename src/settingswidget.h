// This file is part of the NewtonFractal project.
// Copyright (C) 2019 Christian Bauer and Timon Foehl
// License: GNU General Public License version 3 or later,
// see the file LICENSE in the main directory.

#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include "defaults.h"
#include <QMenu>
#include <QWidget>
#include <QMouseEvent>

class RootEdit;
class RootIcon;
struct Parameters;

namespace Ui {
class SettingsWidget;
}

class SettingsWidget : public QWidget
{
	Q_OBJECT

public:
	SettingsWidget(Parameters *params, QWidget *parent = nullptr);
	~SettingsWidget();
	void updateSettings();
	void disableOpenGL();

public slots:
	void toggle();
	void changeSize(QSize size);
	void changeZoom(double factor);
	void addRoot(complex value = complex(0, 0), QColor color = Qt::black);
	void removeRoot(qint8 index = -1);
	void moveRoot(quint8 index, complex value);
	void exportImage();
	void exportSettings();
	void importSettings();

private slots:
	void openRootContextMenu();
	void on_settingsChanged();

protected:
	// Don't pass any events to parent while settings are shown
	void mousePressEvent(QMouseEvent *event) override { event->accept(); }
	void mouseMoveEvent(QMouseEvent *event) override { event->accept(); }
	void mouseReleaseEvent(QMouseEvent *event) override { event->accept(); }
	void resizeEvent(QResizeEvent *event) override { event->accept(); }
	void wheelEvent(QWheelEvent *event) override { event->accept(); }

signals:
	void paramsChanged();
	void sizeChanged(QSize size);
	void exportImageTo(const QString &dir);
	void exportSettingsTo(const QString &dir);
	void importSettingsFrom(const QString &file);
	void reset();

private:
	Ui::SettingsWidget *ui_;
	Parameters *params_;
	QMenu *rootMenu_;
	QList<RootEdit*> rootEdits_;
	QList<RootIcon*> rootIcons_;
};

#endif // SETTINGSWIDGET_H
