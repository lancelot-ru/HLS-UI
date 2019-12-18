#pragma once

#include <QPair>
#include <QString>

class Utils
{
public:
    static QString parseLine(const QString &line, const QString &key)
    {
        int keyStart = line.indexOf(key);
        /// для различения BANDWIDTH и AVERAGE-BANDWIDTH
        if( line.at(keyStart - 1) == "-" )
        {
            keyStart = line.indexOf(key, keyStart + 1);
        }
        if( keyStart > 0 )
        {
            int keyEnd = line.indexOf(",", keyStart);
            if( !QString::compare(key, QString("CODECS")) )
            {
                keyEnd = line.indexOf("\",", keyStart);
            }
            int val = keyStart + key.length() + 1;
            if( keyEnd != -1 )
            {
                return line.mid(val, keyEnd - val).remove("\"");
            }
            else
            {
                return line.mid(val).remove("\"");
            }
        }
        return QString();
    }

    static bool isHLS(const QString &line)
    {
        return !QString::compare(line, QString("#EXTM3U"));
    }

    static QString getReadableCodec(const QString &codec)
    {
        if( codec.startsWith("avc", Qt::CaseInsensitive) ) return "AVC";
        if( codec.startsWith("mp4a", Qt::CaseInsensitive) ) return "AAC";
        if( codec.startsWith("ac-3", Qt::CaseInsensitive) ) return "AC-3";
        if( codec.startsWith("ec-3", Qt::CaseInsensitive) ) return "EC-3";

        return codec;
    }

    static QPair<int, int> getStreamNumbers(const QString &str)
    {
        int first = str.indexOf('#');
        int firstEnd = str.indexOf(' ', first);
        int x = str.mid(first + 1, firstEnd - first - 1).toInt();
        int second = str.indexOf('#', first + 1);
        int secondEnd = str.indexOf(' ', second);
        int y = str.mid(second + 1, secondEnd - second - 1).toInt();

        return qMakePair(x, y);
    }
};
