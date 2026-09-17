#pragma once
#include <QString>
#include <QImage>
#include <QRectF>
#include <vector>
#include <memory>
#include <mutex>

struct Detection {
    QRectF  bbox;
    float   conf = 0.0f;
    int     classId = 0;
    QString className;
};

class TrtYolo {
public:
    TrtYolo();
    ~TrtYolo();

    bool loadEngine(const QString &enginePath);
    void unloadEngine();
    bool isLoaded() const;
    QString enginePath() const { return m_enginePath; }
    QString lastError() const;

    std::vector<Detection> detect(const QImage &img, float confThresh = 0.25f, float iouThresh = 0.45f);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    QString m_enginePath;
    mutable QString m_lastError;
    mutable std::mutex m_mutex;
};
