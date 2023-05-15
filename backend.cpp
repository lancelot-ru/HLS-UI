#include "backend.h"

#include <QFuture>
#include <QNetworkReply>
#include <QtConcurrent>

#include "utils.h"

const int AUDIO_ROWS = 0;
const int AUDIO_COLUMNS = 5;
const int VIDEO_ROWS = 0;
const int VIDEO_COLUMNS = 5;
const int LOG_ROWS = 0;
const int LOG_COLUMNS = 1;

Backend::Backend(QObject *parent)
    : QObject(parent)
    , mAccessManager(new QNetworkAccessManager(parent))
    , mDeviation(10)
    , mGlobalCounter(0)
    , mVideoWatcher(nullptr)
    , mAudioWatcher(nullptr)
{
    connect(this, &Backend::allRepliesFinished, this, &Backend::onAllRepliesFinished);

    createModels();
}

QStandardItemModel *Backend::audioModel()
{
    return mAudioModel;
}

QStandardItemModel *Backend::videoModel()
{
    return mVideoModel;
}

QStandardItemModel *Backend::logModel()
{
    return mLogModel;
}

void Backend::setDeviation(qreal deviation)
{
    mDeviation = deviation;
}

void Backend::reset()
{
    mAudioModel->removeRows(0, mAudioModel->rowCount());
    mVideoModel->removeRows(0, mVideoModel->rowCount());
    mLogModel->removeRows(0, mLogModel->rowCount());

    mUrlsForAudio.clear();
    mUrlsForVideo.clear();
    mVariantStreams.clear();

    mGlobalCounter = 0;

    delete mVideoWatcher;
    delete mAudioWatcher;
    mVideoWatcher = new QFutureWatcher<void>(this);
    mAudioWatcher = new QFutureWatcher<void>(this);
}

void Backend::parseUrl(const QString &url)
{
    QNetworkRequest request(url);
    QNetworkReply *reply = mAccessManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        onReplyFinished(reply);
    });
    if( reply->isFinished() )
    {
        onReplyFinished(reply);
    }
}

void Backend::onReplyFinished(QNetworkReply *reply)
{
    if( reply->error() != QNetworkReply::NoError )
    {
        qDebug() << "Error: " << reply->errorString();
        emit error(reply->errorString());
        return;
    }
    else
    {
        QUrl base = reply->url().adjusted(QUrl::RemoveFilename);
        QByteArray data = reply->readAll();
        QTextStream stream(&data);

        QString line = stream.readLine();
        if( !Utils::isHLS(line) )
        {
            emit error("Неверный формат!");
            return;
        }

        bool isMaster = false;
        while( !stream.atEnd() )
        {
            line = stream.readLine();
            if( line.contains("EXT-X-STREAM-INF") )
            {
                isMaster = true;
                QString average = Utils::parseLine(line, QString("AVERAGE-BANDWIDTH"));
                QString codec = Utils::parseLine(line, QString("CODECS"));
                QString aud = Utils::parseLine(line, QString("AUDIO"));
                QString resolution = Utils::parseLine(line, QString("RESOLUTION"));
                QString bandwidth = Utils::parseLine(line, QString("BANDWIDTH"));
                QString framerate = Utils::parseLine(line, QString("FRAME-RATE"));

                line = stream.readLine();
                if( line.contains(".m3u8") )
                {
                    QUrl final = base.resolved(QUrl(line));
                    VariantStream variantStream;
                    variantStream.averageBandwidth = average.toUInt();
                    variantStream.peakBandwidth = bandwidth.toUInt();
                    variantStream.audio = aud;
                    variantStream.videoStream.url = final.toString();
                    if( codec.contains(",") )
                    {
                        variantStream.videoStream.codec = Utils::getReadableCodec(codec.split(",").value(0));
                        variantStream.audioStream.codec = Utils::getReadableCodec(codec.split(",").value(1));
                    }
                    else
                    {
                        variantStream.videoStream.codec = Utils::getReadableCodec(codec);
                    }
                    variantStream.videoStream.resolution = resolution;
                    variantStream.videoStream.framerate = framerate;
                    mVariantStreams.append(variantStream);

                    if( !mUrlsForVideo.contains(variantStream.videoStream.url) )
                    {
                        mUrlsForVideo.append(variantStream.videoStream.url);
                    }
                }
            }
            if( line.contains("EXT-X-MEDIA:TYPE=AUDIO") )
            {
                QString groupId = Utils::parseLine(line, QString("GROUP-ID"));
                QString numOfChannels = Utils::parseLine(line, QString("CHANNELS"));
                QString language = Utils::parseLine(line, QString("LANGUAGE"));
                QUrl final = base.resolved(QUrl(Utils::parseLine(line, QString("URI"))));
                mUrlsForAudio.append(final.toString());
                for( int i = 0; i < mVariantStreams.size(); ++i )
                {
                    if( mVariantStreams.at(i).audio == groupId )
                    {
                        mVariantStreams[i].audioStream.url = final.toString();
                        mVariantStreams[i].audioStream.language = language;
                        mVariantStreams[i].audioStream.numOfChannels = numOfChannels.toUInt();
                    }
                }
            }
        }

        if( isMaster )
        {
            foreach(auto &url, mUrlsForVideo)
            {
                QNetworkRequest request(url);
                QNetworkReply *newReply = mAccessManager->get(request);
                connect(newReply, &QNetworkReply::finished, this, [this, newReply]()
                {
                    mVideoReplies.append(qMakePair(newReply, 0));
                    mGlobalCounter++;
                    if( mGlobalCounter == mUrlsForAudio.size() + mUrlsForVideo.size() )
                    {
                        emit allRepliesFinished();
                    }
                });
            }

            foreach(auto &audioUrl, mUrlsForAudio)
            {
                QNetworkRequest request(audioUrl);
                QNetworkReply *newReply = mAccessManager->get(request);
                connect(newReply, &QNetworkReply::finished, this, [this, newReply]()
                {
                    mAudioReplies.append(qMakePair(newReply, 0));
                    mGlobalCounter++;
                    if( mGlobalCounter == mUrlsForAudio.size() + mUrlsForVideo.size() )
                    {
                        emit allRepliesFinished();
                    }
                });
            }
        }
    }
    reply->deleteLater();
}

void Backend::onAllRepliesFinished()
{
    mGlobalCounter = 0;
    auto getVideoBitrate = [](QPair<QNetworkReply *, quint32> &pair)
    {
        if( pair.first->error() != QNetworkReply::NoError )
        {
            qDebug() << "Error: " << pair.first->errorString();
            return;
        }
        else
        {
            //QUrl base = pair.first->url().adjusted(QUrl::RemoveFilename);
            QByteArray data = pair.first->readAll();
            QTextStream textStream(&data);

            QString line = textStream.readLine();
            if( !Utils::isHLS(line) )
            {
                qDebug() << "Неверный формат!";
                return;
            }

            quint32 br = 0;
            quint32 count = 0;
            while( !textStream.atEnd() )
            {
                line = textStream.readLine();
                if( line.contains("EXT-X-BITRATE") )
                {
                    QString bitrate = Utils::parseLine(line, QString("EXT-X-BITRATE"));
                    br += bitrate.toUInt();
                    count++;
                }
            }

            if (count > 0)
                pair.second = (br * 1000) / count;
        }
    };
    QFuture<void> videoFuture = QtConcurrent::map(mVideoReplies, getVideoBitrate);

    auto getAudioBitrate = [](QPair<QNetworkReply *, quint32> &pair)
    {
        if( pair.first->error() != QNetworkReply::NoError )
        {
            qDebug() << "Error: " << pair.first->errorString();
            return;
        }
        else
        {
            //QUrl base = pair.first->url().adjusted(QUrl::RemoveFilename);
            QByteArray data = pair.first->readAll();
            QTextStream textStream(&data);

            QString line = textStream.readLine();
            if( !Utils::isHLS(line) )
            {
                qDebug() << "Неверный формат!";
                return;
            }

            quint32 br = 0;
            quint32 count = 0;
            while( !textStream.atEnd() )
            {
                line = textStream.readLine();
                if( line.contains("EXT-X-BITRATE") )
                {
                    QString bitrate = Utils::parseLine(line, QString("EXT-X-BITRATE"));
                    br += bitrate.toUInt();
                    count++;
                }
            }

            if (count > 0)
                pair.second = (br * 1000) / count;
        }
    };
    QFuture<void> audioFuture = QtConcurrent::map(mAudioReplies, getAudioBitrate);

    connect(mVideoWatcher, &QFutureWatcher<void>::finished, this, &Backend::onVideoBitratesComputed);
    mVideoWatcher->setFuture(videoFuture);
    connect(mAudioWatcher, &QFutureWatcher<void>::finished, this, &Backend::onAudioBitratesComputed);
    mAudioWatcher->setFuture(audioFuture);
}

void Backend::onVideoBitratesComputed()
{
    for( int i = 0; i < mVideoReplies.size(); ++i )
    {
        setVideoBitrateByUrl(mVideoReplies.at(i).first->url().toString(), mVideoReplies.at(i).second);
        mVideoReplies.at(i).first->deleteLater();

        mGlobalCounter++;
        if( mGlobalCounter == mUrlsForAudio.size() + mUrlsForVideo.size() )
        {
            mVideoReplies.clear();
            mAudioReplies.clear();
            setModelData();
            break;

        }
    }
}

void Backend::onAudioBitratesComputed()
{
    for( int i = 0; i < mAudioReplies.size(); ++i )
    {
        setAudioBitrateByAudio(mAudioReplies.at(i).first->url().toString(), mAudioReplies.at(i).second);
        mAudioReplies.at(i).first->deleteLater();

        mGlobalCounter++;
        if( mGlobalCounter == mUrlsForAudio.size() + mUrlsForVideo.size() )
        {
            mVideoReplies.clear();
            mAudioReplies.clear();
            setModelData();
            break;
        }
    }
}

void Backend::createModels()
{
    mAudioModel = new QStandardItemModel(AUDIO_ROWS, AUDIO_COLUMNS, this);
    mAudioModel->setHeaderData(0, Qt::Horizontal, tr("Поток"));
    mAudioModel->setHeaderData(1, Qt::Horizontal, tr("Кодек"));
    mAudioModel->setHeaderData(2, Qt::Horizontal, tr("Количество каналов"));
    mAudioModel->setHeaderData(3, Qt::Horizontal, tr("Язык"));
    mAudioModel->setHeaderData(4, Qt::Horizontal, tr("Битрейт"));

    mVideoModel = new QStandardItemModel(VIDEO_ROWS, VIDEO_COLUMNS, this);
    mVideoModel->setHeaderData(0, Qt::Horizontal, tr("Поток"));
    mVideoModel->setHeaderData(1, Qt::Horizontal, tr("Кодек"));
    mVideoModel->setHeaderData(2, Qt::Horizontal, tr("Разрешение"));
    mVideoModel->setHeaderData(3, Qt::Horizontal, tr("Битрейт"));
    mVideoModel->setHeaderData(4, Qt::Horizontal, tr("Фреймрейт"));

    mLogModel = new QStandardItemModel(LOG_ROWS, LOG_COLUMNS, this);
    mLogModel->setHeaderData(0, Qt::Horizontal, tr("Сообщение"));
}

void Backend::setModelData()
{
    for(int i = 0; i < mVariantStreams.size(); ++i )
    {
        if( !mVariantStreams.at(i).videoStream.url.isEmpty() &&
                mVideoModel->findItems(mVariantStreams.at(i).videoStream.url).isEmpty() )
        {
            int rowCount = mVideoModel->rowCount();
            mVideoModel->insertRows(rowCount, 1, QModelIndex());

            mVideoModel->setData(mVideoModel->index(rowCount, 0, QModelIndex()), mVariantStreams.at(i).videoStream.url);
            mVideoModel->setData(mVideoModel->index(rowCount, 1, QModelIndex()), mVariantStreams.at(i).videoStream.codec);
            mVideoModel->setData(mVideoModel->index(rowCount, 2, QModelIndex()), mVariantStreams.at(i).videoStream.resolution);
            mVideoModel->setData(mVideoModel->index(rowCount, 3, QModelIndex()), mVariantStreams.at(i).videoStream.realVideoBitrate);
            mVideoModel->setData(mVideoModel->index(rowCount, 4, QModelIndex()), mVariantStreams.at(i).videoStream.framerate);

            if( mVariantStreams.at(i).videoStream.realVideoBitrate == 0 )
            {
                QString error = QString("Реальный битрейт равен нулю для видео-потока #%1").arg(rowCount + 1);

                rowCount = mLogModel->rowCount();
                mLogModel->insertRows(rowCount, 1, QModelIndex());
                mLogModel->setData(mLogModel->index(rowCount, 0, QModelIndex()), error);
            }
        }
        if( !mVariantStreams.at(i).audioStream.url.isEmpty() &&
                mAudioModel->findItems(mVariantStreams.at(i).audioStream.url).isEmpty() )
        {
            int rowCount = mAudioModel->rowCount();
            mAudioModel->insertRows(rowCount, 1, QModelIndex());

            mAudioModel->setData(mAudioModel->index(rowCount, 0, QModelIndex()), mVariantStreams.at(i).audioStream.url);
            mAudioModel->setData(mAudioModel->index(rowCount, 1, QModelIndex()), mVariantStreams.at(i).audioStream.codec);
            mAudioModel->setData(mAudioModel->index(rowCount, 2, QModelIndex()), mVariantStreams.at(i).audioStream.numOfChannels);
            mAudioModel->setData(mAudioModel->index(rowCount, 3, QModelIndex()), mVariantStreams.at(i).audioStream.language);
            mAudioModel->setData(mAudioModel->index(rowCount, 4, QModelIndex()), mVariantStreams.at(i).audioStream.realAudioBitrate);

            if( mVariantStreams.at(i).audioStream.realAudioBitrate == 0 )
            {
                QString error = QString("Реальный битрейт равен нулю для аудио-потока #%1").arg(rowCount + 1);

                rowCount = mLogModel->rowCount();
                mLogModel->insertRows(rowCount, 1, QModelIndex());
                mLogModel->setData(mLogModel->index(rowCount, 0, QModelIndex()), error);
            }
        }
    }

    for( int i = 0; i < mVariantStreams.size(); ++i )
    {
        if( !mVariantStreams[i].isInRange(mDeviation) )
        {
            int video = !mVideoModel->findItems(mVariantStreams.at(i).videoStream.url).isEmpty()
                    ? mVideoModel->findItems(mVariantStreams.at(i).videoStream.url).first()->index().row() + 1
                    : 0;
            int audio = !mAudioModel->findItems(mVariantStreams.at(i).audioStream.url).isEmpty()
                    ? mAudioModel->findItems(mVariantStreams.at(i).audioStream.url).first()->index().row() + 1
                    : 0;
            QString error;
            if( video == 0 && audio == 0 )
                continue;
            if( video == 0 && audio != 0 )
                error = QString("Реальный битрейт вне допустимого диапазона для Variant Stream'а с аудио-потоком #%1").arg(audio);
            if( video != 0 && audio == 0 )
                error = QString("Реальный битрейт вне допустимого диапазона для Variant Stream'а с видео-потоком #%1").arg(video);
            if( video != 0 && audio != 0 )
                error = QString("Реальный битрейт вне допустимого диапазона для Variant Stream'а с видео-потоком #%1 и аудио-потоком #%2")
                        .arg(video).arg(audio);

            int rowCount = mLogModel->rowCount();
            mLogModel->insertRows(rowCount, 1, QModelIndex());
            mLogModel->setData(mLogModel->index(rowCount, 0, QModelIndex()), error);
        }
    }

    emit analysisFinished();
}

void Backend::setVideoBitrateByUrl(const QString &videoUrl, quint32 videoBitrate)
{
    for(int i = 0; i < mVariantStreams.size(); ++i )
    {
        if( mVariantStreams.at(i).videoStream.url == videoUrl )
            mVariantStreams[i].videoStream.realVideoBitrate = videoBitrate;
    }
}

void Backend::setAudioBitrateByAudio(const QString &audioUrl, quint32 audioBitrate)
{
    for(int i = 0; i < mVariantStreams.size(); ++i )
    {
        if( mVariantStreams.at(i).audioStream.url == audioUrl )
            mVariantStreams[i].audioStream.realAudioBitrate = audioBitrate;
    }
}
