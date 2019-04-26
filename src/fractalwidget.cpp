#include "fractalwidget.h"
#include <QApplication>
#include <QMouseEvent>
#include <QDateTime>
#include <QPainter>

Dragger::Dragger() :
	mode(NoDragging),
	index(-1)
{
}

FractalWidget::FractalWidget(QWidget *parent) :
	QWidget(parent),
	params_(Parameters(DRC)),
	fps_(0)
{
	// Enable mouse movements and connect render thread
	setMouseTracking(true);
	timer_.setInterval(DTI);
	timer_.setSingleShot(true);
	connect(&renderThread_, &RenderThread::fractalRendered, this, &FractalWidget::updateFractal);
	connect(&timer_, &QTimer::timeout, [this]() {
		params_.resize(dragger_.previousSize);
		updateParams(params_);
	});
}

Parameters FractalWidget::params() const
{
	// Return current params
	return params_;
}

void FractalWidget::updateParams(Parameters params)
{
	// Pass params to render thread
	params_ = params;
	renderThread_.render(params_);
}

void FractalWidget::reset()
{
	// Reset roots to be equidistant
	quint8 rootCount = params_.roots.size();
	params_.roots = equidistantRoots(rootCount);
	params_.limits.reset(params_.size);
	updateParams(params_);
	for (quint8 i = 0; i < rootCount; ++i) {
		emit rootMoved(i, params_.roots[i]);
	}
}

void FractalWidget::exportTo(const QString &exportDir)
{
	// Export fractal to file
	QString filePath = exportDir + "/fractal_" +
		QDateTime::currentDateTime().toString("yyMMdd_HHmmss_") +
		QString::number(params_.roots.size()) + "roots_" +
		QString::number(params_.size.width()) + "x" + QString::number(params_.size.width()) + ".png";
	QFile f(filePath);
	f.open(QIODevice::WriteOnly | QIODevice::Truncate);
	pixmap_.save(&f, "png");
}

void FractalWidget::updateFractal(const QPixmap &pixmap, double fps)
{
	// Update
	pixmap_ = pixmap;
	fps_ = fps;
	update();
}

void FractalWidget::paintEvent(QPaintEvent *)
{
	// Static colors
	static const QPen whitePen(Qt::white);
	static const QPen rootPen(Qt::white, 2);
	static const QBrush opaqueBrush(QColor(0, 0, 0, 128));

	// Paint fractal
	QPainter painter(this);
	QString text;
	painter.setRenderHint(QPainter::Antialiasing);
	painter.fillRect(rect(), Qt::darkGray);
	painter.setBrush(opaqueBrush);

	// Draw pixmap if rendered yet
	if (!pixmap_.isNull()) {
		painter.drawPixmap(rect(), pixmap_);
		text = "F1 → Settings [" + QString::number(fps_, 'f', 2) + "fps]";

		// Draw roots
		rootPoints_.clear();
		painter.setPen(rootPen);
		quint8 rootCount = params_.roots.size();
		for (quint8 i = 0; i < rootCount; ++i) {
			QPoint point = complex2point(params_.roots[i], pixmap_.rect(), rect(), params_.limits);
			painter.drawEllipse(point, RR, RR);
			rootPoints_.append(point);
		}

	} else text = tr("Rendering initial image, please wait...");

	// Draw text
	QFontMetrics metrics = painter.fontMetrics();
	int textWidth = metrics.horizontalAdvance(text);
	painter.setPen(Qt::NoPen);
	painter.drawRect((width() - textWidth) / 2 - 6, 0, textWidth + 10, metrics.lineSpacing() + 6);
	painter.setPen(whitePen);
	painter.drawText((width() - textWidth) / 2, metrics.leading() + metrics.ascent() + 1, text);
}

void FractalWidget::mousePressEvent(QMouseEvent *event)
{
	// Overwrite size with scaleSize for smooth moving
	dragger_.previousSize = params_.size;
	params_.resize(params_.size * params_.scaleDown);

	// Check if mouse press is on root
	QPoint pos = event->pos();
	quint8 rootCount = rootPoints_.length();
	for (quint8 i = 0; i < rootCount; ++i) {
		if (rootContainsPoint(rootPoints_[i], pos)) {
			setCursor(Qt::ClosedHandCursor);
			dragger_.mode = DraggingRoot;
			dragger_.index = i;
			return;
		}
	}

	// Else dragging fractal
	setCursor(Qt::SizeAllCursor);
	dragger_.mode = DraggingFractal;
	dragger_.previousPos = event->pos();
}

void FractalWidget::mouseMoveEvent(QMouseEvent *event)
{
	// Move root if dragging
	QPoint pos = event->pos();
	if (dragger_.mode == DraggingRoot && dragger_.index >= 0 && dragger_.index < rootPoints_.length()) {
		params_.roots[dragger_.index] = point2complex(pos, pixmap_.rect(), rect(), params_.limits);
		updateParams(params_);
		emit rootMoved(dragger_.index, params_.roots[dragger_.index]);

	// Else move fractal if dragging
	} else if (dragger_.mode == DraggingFractal) {
		complex d = distance2complex(dragger_.previousPos - event->pos(), pixmap_.rect(), rect(), params_.limits);
		params_.limits.move(d.real(), d.imag());
		updateParams(params_);
		dragger_.previousPos = event->pos();

	// Else if event over root change cursor
	} else {
		quint8 rootCount = rootPoints_.length();
		for (quint8 i = 0; i < rootCount; ++i) {
			if (rootContainsPoint(rootPoints_[i], pos)) {
				setCursor(Qt::OpenHandCursor);
				return;
			}
		}
		setCursor(Qt::ArrowCursor);
	}
}

void FractalWidget::mouseReleaseEvent(QMouseEvent *event)
{
	// Reset dragging and render actual size
	Q_UNUSED(event);
	params_.resize(dragger_.previousSize);
	dragger_.mode = NoDragging;
	dragger_.index = -1;
	updateParams(params_);
}

void FractalWidget::resizeEvent(QResizeEvent *event)
{
	// Change resolution on resize
	params_.resize(event->size(), true);
	updateParams(params_);
}

void FractalWidget::wheelEvent(QWheelEvent *event)
{
	// Calculate weight
	double xw = 0.5, yw = 0.5;
	if (params_.zoomToCursor) {
		xw = (double)event->pos().x() / width();
		yw = (double)event->pos().y() / height();
	}

	// Overwrite size with scaleSize for smooth moving
	if (!timer_.isActive()) {
		dragger_.previousSize = params_.size;
		params_.resize(params_.size * params_.scaleDown);
	}
	timer_.start();

	// Zoom fractal in / out
	bool in = event->angleDelta().y() > 0;
	params_.limits.zoom(in, xw, yw);
	updateParams(params_);
}