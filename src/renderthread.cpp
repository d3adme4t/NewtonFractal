#include "renderthread.h"
#include <QtConcurrent>
#include <QImage>
#include <QPixmap>
#include <QDebug>

static const complex step(HS, HS);
static const QColor colors[NR] = { Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta, Qt::yellow };

ImageLine::ImageLine(QRgb *scanLine, int lineIndex, int lineSize, const Parameters &params) :
	scanLine(scanLine),
	lineIndex(lineIndex),
	lineSize(lineSize),
	params(params)
{
}

RenderThread::RenderThread(QObject *parent) :
	QThread(parent),
	abort_(false)
{
}

RenderThread::~RenderThread()
{
	// Abort and wait for thread
	mutex_.lock();
	abort_ = true;
	condition_.wakeOne();
	mutex_.unlock();
	wait();
}

void RenderThread::render(Parameters params)
{
	// Lock mutex
	QMutexLocker locker(&mutex_);
	nextParams_ = params;
	if (nextParams_.roots.length() > NR) {
		nextParams_.roots.resize(NR);
	}

	// Start working
	if (!isRunning()) {
		start(QThread::NormalPriority);
	}
}

void RenderThread::run()
{
	// Run foooooorrreeeeevvvvvveeeeer
	while (1) {

		// Get new parameters
		mutex_.lock();
		currentParams_ = nextParams_;
		mutex_.unlock();

		// Create image for fast pixel IO
		QList<ImageLine> lineList;
		QImage image(currentParams_.resultSize, QImage::Format_RGB32);
		qint32 height = currentParams_.resultSize.height();

		// Iterate y-pixels
		for (int y = 0; y < height; ++y) {
			ImageLine il((QRgb*)(image.scanLine(y)), y, image.width(), currentParams_);
			il.zy = y * (YB - YA) / (height - 1) + YA;
			lineList.append(il);
		}

		// Iterate x-pixels with multiple threads
		QtConcurrent::blockingMap(lineList, iterateX);

		// Return if aborted, else emit signal
		if (abort_) return;
		emit fractalRendered(QPixmap::fromImage(image));
	}
}

void iterateX(ImageLine &il)
{
	// Iterate x-pixels
	quint8 rootCount = il.params.roots.size();

	for (int x = 0; x < il.lineSize; ++x) {

		// Create complex number from current pixel
		il.zx = x * (XB - XA) / (il.lineSize - 1) + XA;
		complex z(il.zx, il.zy);

		// Newton iteration
		for (quint8 i = 0; i < il.params.maxIterations; ++i) {
			complex dz = (func(z + step, il.params.roots) - func(z, il.params.roots)) / step;
			complex z0 = z - func(z, il.params.roots) / dz;

			// If root has been found check which
			if (abs(z0 - z) < EPS) {
				for (quint8 r = 0; r < rootCount; ++r) {
					if (abs(z0 - il.params.roots[r]) < EPS) {
						QColor c = colors[r].darker(50 + i * 10);
						il.scanLine[x] = c.rgb();
						break;
					}
				}
				break;

			// Else next iteration
			} else z = z0;
		}
	}
}

complex func(complex z, const RootVector &roots)
{
	// Calculate function with given roots
	complex result(1, 0);
	quint8 rootCount = roots.length();
	for (quint8 i = 0; i < rootCount; ++i) {
		result = result * (z - roots[i]);
	}
	return result;
}
