#pragma once

#include <QObject>

#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QStandardItemModel>

struct VideoStream
{
    QString url;
    QString codec;
    QString resolution;
    quint32 realVideoBitrate; //in bits per second
    QString framerate;
};

struct AudioStream
{
    QString url;
    QString codec;
    quint32 numOfChannels;
    QString language;
    quint32 realAudioBitrate; //in bits per second
};

struct VariantStream
{
    quint32 averageBandwidth; //in bits per second
    quint32 peakBandwidth; //in bits per second
    QString audio;

    VideoStream videoStream;
    AudioStream audioStream;

    bool isInRange(qreal deviation)
    {
        quint32 x;
        if( videoStream.realVideoBitrate + audioStream.realAudioBitrate > averageBandwidth )
            x = videoStream.realVideoBitrate + audioStream.realAudioBitrate - averageBandwidth;
        else
            x = averageBandwidth - videoStream.realVideoBitrate - audioStream.realAudioBitrate;
        qreal y = x / static_cast<qreal>(averageBandwidth);
        qreal z = deviation / static_cast<qreal>(100);

        return z > y;
    }
};

class Backend : public QObject
{
    Q_OBJECT
public:
    explicit Backend(QObject *parent = nullptr);

    QStandardItemModel *audioModel();
    QStandardItemModel *videoModel();
    QStandardItemModel *logModel();

    void setDeviation(qreal deviation);
    void reset();
    void parseUrl(const QString &url);

signals:
    void analysisFinished();
    void error(const QString &errorString);
    void allRepliesFinished();

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onAllRepliesFinished();
    void onVideoBitratesComputed();
    void onAudioBitratesComputed();

private:
    void createModels();
    void setModelData();
    void setVideoBitrateByUrl(const QString &videoUrl, quint32 videoBitrate);
    void setAudioBitrateByAudio(const QString &audioUrl, quint32 audioBitrate);

private:
    QStandardItemModel *mAudioModel;
    QStandardItemModel *mVideoModel;
    QStandardItemModel *mLogModel;

    QNetworkAccessManager *mAccessManager;

    // in percent
    qreal mDeviation;

    QList<QString> mUrlsForVideo;
    QList<QString> mUrlsForAudio;
    QList<VariantStream> mVariantStreams;

    int mGlobalCounter;

    QList<QPair<QNetworkReply*, quint32> > mVideoReplies;
    QList<QPair<QNetworkReply*, quint32> > mAudioReplies;
    QFutureWatcher<void> *mVideoWatcher;
    QFutureWatcher<void> *mAudioWatcher;
};
