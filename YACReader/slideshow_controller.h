#ifndef SLIDESHOW_CONTROLLER_H
#define SLIDESHOW_CONTROLLER_H

#include <QObject>

class QTimer;

class SlideshowController : public QObject
{
    Q_OBJECT

public:
    enum State {
        Stopped,
        Playing,
        Paused
    };
    Q_ENUM(State)

    explicit SlideshowController(QObject *parent = nullptr);
    ~SlideshowController() override;

    State state() const;
    qreal interval() const;
    bool loop() const;

    void setInterval(qreal seconds);
    void setLoop(bool loop);

public slots:
    void toggle();
    void stop();
    void faster();
    void slower();

signals:
    void stateChanged(State newState);
    void advancePage();
    void intervalChanged(qreal newInterval);

private slots:
    void onTimeout();

private:
    QTimer *timer;
    State mState = Stopped;
    qreal mInterval = 3.0;
    bool mLoop = false;

    static constexpr qreal MIN_INTERVAL = 0.5;
    static constexpr qreal MAX_INTERVAL = 30.0;
    static constexpr qreal INTERVAL_STEP = 0.5;

    void startTimer();
    void stopTimer();
};

#endif
