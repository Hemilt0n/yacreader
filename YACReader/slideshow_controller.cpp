#include "slideshow_controller.h"
#include "configuration.h"

#include <QTimer>

SlideshowController::SlideshowController(QObject *parent)
    : QObject(parent)
    , timer(new QTimer(this))
{
    mInterval = Configuration::getConfiguration().getSlideshowInterval();
    mLoop = Configuration::getConfiguration().getSlideshowLoop();
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this, &SlideshowController::onTimeout);
}

SlideshowController::~SlideshowController() = default;

SlideshowController::State SlideshowController::state() const { return mState; }
qreal SlideshowController::interval() const { return mInterval; }
bool SlideshowController::loop() const { return mLoop; }

void SlideshowController::setInterval(qreal seconds)
{
    mInterval = qBound(MIN_INTERVAL, seconds, MAX_INTERVAL);
    Configuration::getConfiguration().setSlideshowInterval(mInterval);
    emit intervalChanged(mInterval);

    if (mState == Playing) {
        timer->setInterval(qRound(mInterval * 1000));
    }
}

void SlideshowController::setLoop(bool loop)
{
    mLoop = loop;
    Configuration::getConfiguration().setSlideshowLoop(mLoop);
}

void SlideshowController::toggle()
{
    switch (mState) {
    case Stopped:
    case Paused:
        mState = Playing;
        startTimer();
        break;
    case Playing:
        mState = Paused;
        stopTimer();
        break;
    }
    emit stateChanged(mState);
}

void SlideshowController::stop()
{
    if (mState == Stopped)
        return;
    mState = Stopped;
    stopTimer();
    emit stateChanged(mState);
}

void SlideshowController::faster()
{
    setInterval(mInterval - INTERVAL_STEP);
}

void SlideshowController::slower()
{
    setInterval(mInterval + INTERVAL_STEP);
}

void SlideshowController::onTimeout()
{
    emit advancePage();
}

void SlideshowController::startTimer()
{
    timer->start(qRound(mInterval * 1000));
}

void SlideshowController::stopTimer()
{
    timer->stop();
}
