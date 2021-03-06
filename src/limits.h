// This file is part of the NewtonFractal project.
// Copyright (C) 2019 Christian Bauer and Timon Foehl
// License: GNU General Public License version 3 or later,
// see the file LICENSE in the main directory.

#ifndef LIMITS_H
#define LIMITS_H

#include <QSize>
#include <QPoint>
#include <QVector4D>

class Limits
{
public:
	Limits(bool original = false);
	~Limits();
	Limits &operator=(const Limits &other);
	bool operator==(const Limits &other) const;
	bool operator!=(const Limits &other) const;
	void move(QPoint distance, const QSize &ref);
	void zoom(bool in, double xw, double yw);
	void reset(QSize size);
	void resize(QSize delta);
	void set(double left, double right, double top, double bottom);
	void setOriginal(double left, double right, double top, double bottom);

	double width() const;
	double height() const;
	double left() const;
	double right() const;
	double top() const;
	double bottom() const;
	QVector4D vec4() const;
	double zoomFactor() const;
	void setZoomFactor(double zoomFactor);
	const Limits *original() const;

private:
	double left_;
	double right_;
	double top_;
	double bottom_;
	Limits *original_;
};

#endif // LIMITS_H
