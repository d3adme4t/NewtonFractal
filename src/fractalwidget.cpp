// This file is part of the NewtonFractal project.
// Copyright (C) 2019 Christian Bauer and Timon Foehl
// License: GNU General Public License version 3 or later,
// see the file LICENSE in the main directory.

#include "fractalwidget.h"
#include "settingswidget.h"
#include "parameters.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QShortcut>
#include <QSettings>
#include <QPainter>
#include <QAction>
#include <QIcon>

static QPoint mousePosition;
static const QVector<QVector3D> vertices = QVector<QVector3D>() <<
	QVector3D(-1, -1, 0) << QVector3D(1, -1, 0) << QVector3D(1, 1, 0) << QVector3D(-1, 1, 0);

Dragger::Dragger() :
	mode(NoDragging),
	index(-1)
{
}

FractalWidget::FractalWidget(QWidget *parent) :
	QOpenGLWidget(parent),
	params_(new Parameters()),
	settingsWidget_(new SettingsWidget(params_, this)),
	fps_(0),
	legend_(true),
	position_(false)
{
	// Initialize layout
	QSpacerItem *spacer = new QSpacerItem(nf::DSI / 2.0, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
	QHBoxLayout *centralLayout = new QHBoxLayout(this);
	centralLayout->setSpacing(0);
	centralLayout->setContentsMargins(0, 0, 0, 0);
	centralLayout->addItem(spacer);
	centralLayout->addWidget(settingsWidget_);
	setLayout(centralLayout);

	// Add shortcuts and connect them
	QShortcut *quit = new QShortcut(QKeySequence("Ctrl+Q"), this);
	QShortcut *hide = new QShortcut(QKeySequence(Qt::Key_Escape), this);
	QShortcut *orbit = new QShortcut(QKeySequence(Qt::Key_F2), this);
	QShortcut *position = new QShortcut(QKeySequence(Qt::Key_F3), this);
	QShortcut *settings = new QShortcut(QKeySequence(Qt::Key_F1), this);
	QShortcut *resetFractal = new QShortcut(QKeySequence("Ctrl+R"), this);
	QShortcut *exportImage = new QShortcut(QKeySequence("Ctrl+S"), this);
	QShortcut *exportSettings = new QShortcut(QKeySequence("Ctrl+E"), this);
	QShortcut *importSettings = new QShortcut(QKeySequence("Ctrl+I"), this);
	connect(quit, &QShortcut::activated, QApplication::instance(), &QCoreApplication::quit);
	connect(hide, &QShortcut::activated, [this]() { legend_ = !legend_; update(); });
	connect(orbit, &QShortcut::activated, [this]() { params_->orbitMode = !params_->orbitMode; updateParams(); });
	connect(position, &QShortcut::activated, [this]() { position_ = !position_; update(); });
	connect(settings, &QShortcut::activated, settingsWidget_, &SettingsWidget::toggle);
	connect(resetFractal, &QShortcut::activated, settingsWidget_, &SettingsWidget::reset);
	connect(exportImage, &QShortcut::activated, settingsWidget_, &SettingsWidget::exportImage);
	connect(exportSettings, &QShortcut::activated, settingsWidget_, &SettingsWidget::exportSettings);
	connect(importSettings, &QShortcut::activated, settingsWidget_, &SettingsWidget::importSettings);

	// Initialize general stuff
	setMinimumSize(nf::MSI, nf::MSI);
	setMouseTracking(true);
	setWindowTitle(QApplication::applicationName());
	setWindowIcon(QIcon("://resources/icons/icon.png"));
	scaleDownTimer_.setInterval(nf::DTI);
	scaleDownTimer_.setSingleShot(true);
	resize(params_->size);
	settingsWidget_->hide();

	// Connect settingswidget signals
	connect(settingsWidget_, &SettingsWidget::paramsChanged, this, &FractalWidget::updateParams);
	connect(settingsWidget_, &SettingsWidget::sizeChanged, this, QOverload<const QSize &>::of(&FractalWidget::resize));
	connect(settingsWidget_, &SettingsWidget::exportImageRequested, this, &FractalWidget::exportImageTo);
	connect(settingsWidget_, &SettingsWidget::exportSettingsTo, this, &FractalWidget::exportSettingsTo);
	connect(settingsWidget_, &SettingsWidget::importSettingsFrom, this, &FractalWidget::importSettingsFrom);
	connect(settingsWidget_, &SettingsWidget::reset, this, &FractalWidget::reset);

	// Connect renderthread and timer signals
	connect(&renderThread_, &RenderThread::fractalRendered, this, &FractalWidget::updateFractal);
	connect(&renderThread_, &RenderThread::orbitRendered, this, &FractalWidget::updateOrbit);
	connect(&scaleDownTimer_, &QTimer::timeout, [this]() {
		params_->scaleDown = false;
		updateParams();
	});

	// Connect benchmark signals
	connect(settingsWidget_, &SettingsWidget::benchmarkRequested, this, &FractalWidget::runBenchmark);
	connect(&renderThread_, &RenderThread::benchmarkFinished, this, &FractalWidget::finishBenchmark);
	connect(&renderThread_, &RenderThread::benchmarkProgress, settingsWidget_, &SettingsWidget::setBenchmarkProgress);

	// Initialize parameters
	reset();
}

void FractalWidget::updateParams()
{
	// Pass params to renderthread by const reference
	if (isEnabled()) {
		renderThread_.render(*params_);
	}
}

void FractalWidget::exportImageTo(const QString &dir)
{
	// Close settingsWidget
	bool closed = settingsWidget_->isHidden();
	settingsWidget_->setHidden(true);

	// Export fractal to file
	QFile f(dir + "/" + dynamicFileName(*params_, "png"));
	f.open(QIODevice::WriteOnly | QIODevice::Truncate);
	grab().save(&f, "png");

	// Reopen if needed
	settingsWidget_->setHidden(closed);
}

void FractalWidget::exportSettingsTo(const QString &dir)
{
	// Create ini file
	QSettings ini(dir + "/" + dynamicFileName(*params_, "ini"), QSettings::IniFormat, this);

	// General parameters
	ini.beginGroup("Parameters");
	ini.setValue("size", params_->size);
	ini.setValue("maxIterations", params_->maxIterations);
	ini.setValue("damping", complex2string(params_->damping));
	ini.setValue("scaleDownFactor", params_->scaleDownFactor);
	ini.setValue("scaleDown", params_->scaleDown);
	ini.setValue("processor", static_cast<quint8>(params_->processor));
	ini.setValue("orbitMode", params_->orbitMode);
	ini.setValue("orbitStart", params_->orbitStart);
	ini.endGroup();

	// Limits
	ini.beginGroup("Limits");
	ini.setValue("left", params_->limits.left());
	ini.setValue("right", params_->limits.right());
	ini.setValue("top", params_->limits.top());
	ini.setValue("bottom", params_->limits.bottom());
	ini.setValue("left_original", params_->limits.original()->left());
	ini.setValue("right_original", params_->limits.original()->right());
	ini.setValue("top_original", params_->limits.original()->top());
	ini.setValue("bottom_original", params_->limits.original()->bottom());
	ini.endGroup();

	// Roots
	ini.beginGroup("Roots");
	quint8 rootCount = params_->roots.count();
	for (quint8 i = 0; i < rootCount; ++i) {
		ini.setValue(
			"root" + QString::number(i),
			complex2string(params_->roots[i].value(), 10) + " : " + params_->roots[i].color().name()
		);
	}
	ini.endGroup();
}

void FractalWidget::importSettingsFrom(const QString &file)
{
	// Open ini file
	QSettings ini(file, QSettings::IniFormat, this);

	// General parameters
	ini.beginGroup("Parameters");
	params_->size = ini.value("size", QSize(nf::DSI, nf::DSI)).toSize();
	params_->maxIterations = ini.value("maxIterations", nf::DMI).toUInt();
	params_->damping = string2complex(ini.value("damping", complex2string(nf::DDP)).toString());
	params_->scaleDownFactor = ini.value("scaleDownFactor", nf::DSC).toDouble();
	params_->scaleDown = ini.value("scaleDown", false).toBool();
	params_->processor = static_cast<Processor>(ini.value("processor", 1).toUInt());
	params_->orbitMode = ini.value("orbitMode", false).toBool();
	params_->orbitStart = ini.value("orbitStart").toPoint();
	ini.endGroup();

	// Limits
	ini.beginGroup("Limits");
	params_->limits.set(
		ini.value("left", 1).toDouble(), ini.value("right", 1).toDouble(),
		ini.value("top", 1).toDouble(), ini.value("bottom", 1).toDouble());
	params_->limits.original()->set(
		ini.value("left_original", 1).toDouble(), ini.value("right_original", 1).toDouble(),
		ini.value("top_original", 1).toDouble(), ini.value("bottom_original", 1).toDouble());
	ini.endGroup();

	// Update settings
	resize(params_->size);
	settingsWidget_->updateSettings();

	// Remove old roots
	quint8 rootCount = params_->roots.count();
	for (quint8 i = 0; i < rootCount; ++i) {
		settingsWidget_->removeRoot();
	}

	// Add Roots
	ini.beginGroup("Roots");
	for (QString key : ini.childKeys()) {
		QStringList str = ini.value(key).toString().split(":");
		if (str.length() >= 2) {
			settingsWidget_->addRoot(string2complex(str.first()), QColor(str.last().simplified()));
		}
	}
	ini.endGroup();

}

void FractalWidget::reset()
{
	// Reset roots to be equidistant
	params_->reset();
	updateParams();
	settingsWidget_->updateSettings();
}

void FractalWidget::runBenchmark()
{
	// Disable editing and set params
	setEnabled(false);
	params_->benchmark = true;
	params_->scaleDown = false;
	settingsWidget_->showBenchmarkProgress(true);

	// Run benchmark
	benchmarkTimer_.start();
	renderThread_.render(*params_);
}

void FractalWidget::finishBenchmark(const QImage &image)
{
	// Static output string
	static const QString out = "Rendered %1 pixels in:\n%2 hr, %3 min, %4 sec and %5 ms";

	// Get time and number of pixels
	int pixels = image.width() * image.height();
	qint64 elapsed = benchmarkTimer_.elapsed();
	int s = elapsed / 1000;
	int ms = elapsed % 1000;
	int m = s / 60;
	s %= 60;
	int h = m / 60;
	m %= 60;

	// Show stats
	QMessageBox::StandardButton btn = QMessageBox::question(
		this, tr("Benchmark finished"),
		out.arg(pixels).arg(h).arg(m).arg(s).arg(ms),
		QMessageBox::Save | QMessageBox::Cancel);

	// Save image
	if (btn == QMessageBox::Save) {
		QSettings settings;
		QString dir = settings.value("imagedir", QStandardPaths::standardLocations(QStandardPaths::PicturesLocation)).toString();
		dir = QFileDialog::getExistingDirectory(this, tr("Export fractal to"), dir);
		if (!dir.isEmpty()) {
			settings.setValue("imagedir", dir);
			image.save(dir + "/" + dynamicFileName(*params_, "bmp"), "BMP", 100);
		}
	}

	// Enable editing again and reset params
	settingsWidget_->showBenchmarkProgress(false);
	setEnabled(true);
	params_->benchmark = false;
	updateParams();
}

void FractalWidget::updateFractal(const QPixmap &pixmap, double fps)
{
	// Update
	pixmap_ = pixmap;
	fps_ = fps;
	update();
}

void FractalWidget::updateOrbit(const QVector<QPoint> &orbit, double fps)
{
	// Update
	orbit_ = orbit;
	fps_ = fps;
	update();
}

void FractalWidget::initializeGL()
{
	// Check for support
	initializeOpenGLFunctions();
	if (glGetString(GL_VERSION) == 0) {
		settingsWidget_->disableOpenGL();
	}

	// Initialize OpenGL and shader program
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	program_ = new QOpenGLShaderProgram(this);
	program_->addShaderFromSourceFile(QOpenGLShader::Fragment, "://src/fractal.fsh");
	program_->link();
	program_->bind();
	program_->setUniformValue("EPS", float(nf::EPS));
}

void FractalWidget::paintGL()
{
	// Static pens and brushes
	static const QPen circlePen(Qt::white, 2);
	static const QBrush opaqueBrush(QColor(0, 0, 0, 128));

	// Static pixmaps
	static const QPixmap pixHide("://resources/icons/hide.png");
	static const QPixmap pixSettings("://resources/icons/settings.png");
	static const QPixmap pixOrbit("://resources/icons/orbit.png");
	static const QPixmap pixPosition("://resources/icons/position.png");
	static const QPixmap pixFps("://resources/icons/fps.png");

	// Static legend geometry
	static const int spacing = 10;
	static const QFont consolas("Consolas", 12);
	static const QFontMetrics metrics(consolas);
#if QT_VERSION >= 0x050B00 // For any Qt Version >= 5.11.0 metrics.width() is deprecated
	static const int textWidth = 3 * spacing + pixFps.width() + metrics.horizontalAdvance("999.99");
#else
	static const int textWidth = 3 * spacing + pixFps.width() + metrics.width("999.99");
#endif
	static const int textHeight = spacing + 5 * (pixFps.height() + spacing);
	static const QRect legendRect(spacing, spacing, textWidth, textHeight);
	static const QPoint ptHide(legendRect.topLeft() + QPoint(spacing, spacing));
	static const QPoint ptSettings(ptHide + QPoint(0, pixHide.height() + spacing));
	static const QPoint ptOrbit(ptSettings + QPoint(0, pixSettings.height() + spacing));
	static const QPoint ptPosition(ptOrbit + QPoint(0, pixOrbit.height() + spacing));
	static const QPoint ptFps(ptPosition + QPoint(0, pixPosition.height() + spacing));

	// Paint fractal
	QPainter painter(this);
	painter.setFont(consolas);
	painter.setRenderHint(QPainter::Antialiasing);
	glEnable(GL_MULTISAMPLE);

	// Draw pixmap if rendered yet and cpu mode
	if (params_->processor != GPU_OPENGL && !pixmap_.isNull()) {
		painter.drawPixmap(rect(), pixmap_);
	} else {
		// Update params and draw
		quint8 rootCount = params_->roots.count();
		program_->bind();
		program_->enableAttributeArray(0);
		program_->setAttributeArray(0, vertices.constData());
		program_->setUniformValue("rootCount", rootCount);
		program_->setUniformValue("limits", params_->limits.vec4());
		program_->setUniformValue("maxIterations", params_->maxIterations);
		program_->setUniformValue("damping", complex2vec2(params_->damping));
		program_->setUniformValue("size", QVector2D(size().width(), size().height()));
		program_->setUniformValueArray("roots", params_->rootsVec2().constData(), rootCount);
		program_->setUniformValueArray("colors", params_->colorsVec3().constData(), rootCount);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}


	// Circle pen / brush
	painter.setPen(circlePen);
	painter.setBrush(opaqueBrush);

	// Draw roots and legend if enabled
	if (legend_) {

		// Draw roots
		quint8 rootCount = params_->roots.count();
		for (quint8 i = 0; i < rootCount; ++i) {
			QPoint point = params_->complex2point(params_->roots[i].value());
			painter.drawEllipse(point, nf::RIR, nf::RIR);
		}

		// Draw legend
		painter.drawRoundedRect(legendRect, 10, 10);
		painter.drawPixmap(ptHide, pixHide);
		painter.drawPixmap(ptSettings, pixSettings);
		painter.drawPixmap(ptOrbit, pixOrbit);
		painter.drawPixmap(ptPosition, pixPosition);
		painter.drawPixmap(ptFps, pixFps);
		painter.drawText(ptHide + QPoint(pixHide.width() + spacing, metrics.height() - 4), "ESC");
		painter.drawText(ptSettings + QPoint(pixSettings.width() + spacing, metrics.height() - 4), "F1");
		painter.drawText(ptOrbit + QPoint(pixOrbit.width() + spacing, metrics.height() - 4), "F2");
		painter.drawText(ptPosition + QPoint(pixPosition.width() + spacing, metrics.height() - 4), "F3");
		painter.drawText(ptFps + QPoint(pixFps.width() + spacing, metrics.height() - 4), QString::number(fps_, 'f', 2));
	}

	// Draw position if enabled
	if (position_) {
		QString zstr = complex2string(params_->point2complex(mousePosition));
#if QT_VERSION >= 0x050B00 // For any Qt Version >= 5.11.0 metrics.width() is deprecated
		QRect posRect(mousePosition, QSize(metrics.horizontalAdvance(zstr) + spacing, metrics.height() + spacing));
#else
		QRect posRect(mousePosition, QSize(metrics.width(zstr) + spacing, metrics.height() + spacing));
#endif
		painter.drawRoundedRect(posRect, 6, 6);
		painter.drawText(posRect, Qt::AlignCenter, zstr);
	}

	// Draw orbit if enabled
	if (params_->orbitMode) {
		quint32 orbitSize = orbit_.size();
		if (orbitSize >= 1) {
			painter.drawEllipse(orbit_[0], nf::OIR, nf::OIR);
			for (quint32 i = 1; i < orbitSize; ++i) {
				painter.drawLine(orbit_[i - 1], orbit_[i]);
				painter.drawEllipse(orbit_[i], nf::OIR, nf::OIR);
			}
		}
	}
}

void FractalWidget::resizeGL(int w, int h)
{
	// Overwrite size with scaleSize for smooth resizing
	if (!scaleDownTimer_.isActive() && params_->processor != GPU_OPENGL)
		params_->scaleDown = true;

	// Restart timer
	scaleDownTimer_.start();

	// Change resolution on resize
	QSize newSize(w, h);
	params_->resize(newSize);
	glViewport(0, 0, w, h);
	settingsWidget_->changeSize(newSize);
	updateParams();
}

void FractalWidget::mousePressEvent(QMouseEvent *event)
{
	// Set scaleDown, previousPos and orbit
	QPoint pos = event->pos();
	dragger_.previousPos = pos;
	if (params_->processor != GPU_OPENGL)
		params_->scaleDown = true;

	// Check if mouse press is on root
	int i = params_->rootContainsPoint(pos);
	if (i >= 0) {
		setCursor(Qt::ClosedHandCursor);
		dragger_.mode = DraggingRoot;
		dragger_.index = i;
		return;
	}

	// Else dragging fractal
	setCursor(Qt::SizeAllCursor);
	dragger_.mode = DraggingFractal;
}

void FractalWidget::mouseMoveEvent(QMouseEvent *event)
{
	// Move root if dragging
	mousePosition = event->pos();
	if (dragger_.mode == DraggingRoot && dragger_.index >= 0 && dragger_.index < params_->roots.count()) {
		if (event->modifiers() == Qt::KeyboardModifier::ShiftModifier) {
			QPointF distance = mousePosition - dragger_.previousPos;
			params_->roots[dragger_.index] += params_->distance2complex(distance * nf::MOD);
			dragger_.previousPos = mousePosition;
		} else params_->roots[dragger_.index] = params_->point2complex(mousePosition);
		settingsWidget_->moveRoot(dragger_.index, params_->roots[dragger_.index].value());
		updateParams();

	// Else move fractal if dragging
	} else if (dragger_.mode == DraggingFractal) {
		params_->limits.move(dragger_.previousPos - mousePosition, params_->size);
		dragger_.previousPos = event->pos();
		updateParams();

	// Else if event over root change cursor
	} else {
		if (params_->rootContainsPoint(mousePosition) >= 0) {
			setCursor(Qt::OpenHandCursor);
		} else setCursor(Qt::ArrowCursor);
	}

	// Update position if enabled
	if (position_) {
		update();
	}

	// Render orbit if enabled
	if (params_->orbitMode) {
		params_->orbitStart = mousePosition;
		updateParams();
	}
}

void FractalWidget::mouseReleaseEvent(QMouseEvent *event)
{
	// Reset dragging and render actual size
	Q_UNUSED(event);
	params_->scaleDown = false;
	dragger_.mode = NoDragging;
	dragger_.index = -1;
	updateParams();
}

void FractalWidget::wheelEvent(QWheelEvent *event)
{
	// Calculate weight
	double xw = (double)event->pos().x() / width();
	double yw = (double)event->pos().y() / height();

	// Overwrite size with scaleSize for smooth moving
	if (!scaleDownTimer_.isActive() && params_->processor != GPU_OPENGL)
		params_->scaleDown = true;

	// Restart timer
	scaleDownTimer_.start();

	// Zoom fractal in / out
	bool in = event->angleDelta().y() > 0;
	params_->limits.zoom(in, xw, yw);
	settingsWidget_->changeZoom(params_->limits.zoomFactor());
	updateParams();
}
