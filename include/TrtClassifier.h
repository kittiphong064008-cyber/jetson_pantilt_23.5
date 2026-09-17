#pragma once
#include <QString>
#include <QImage>
#include <vector>
#include <memory>
#include <mutex>

struct ClassificationResult {
    int     classId = -1;
    float   confidence = 0.0f;
    QString className;
    bool    valid = false;
};

class TrtClassifier {
public:
    TrtClassifier();
    ~TrtClassifier();

    bool loadEngine(const QString &enginePath);
    void unloadEngine();
    bool isLoaded() const;
    QString enginePath() const { return m_enginePath; }
    QString lastError() const;

    ClassificationResult classify(const QImage &img);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    QString m_enginePath;
    mutable QString m_lastError;
    mutable std::mutex m_mutex;
};
