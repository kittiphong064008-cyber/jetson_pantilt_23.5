#pragma once
#include <QThread>
#include <QImage>
#include <QString>
#include <QStringList>
#include <atomic>
#include <cstddef>
#include <vector>
#include <mutex>

class V4L2Camera : public QThread {
    Q_OBJECT
public:
    explicit V4L2Camera(const QString &device, int width = 640, int height = 480, QObject *parent = nullptr);
    ~V4L2Camera() override;

    void stopCapture();
    QString device() const { return m_device; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    static QStringList availableDevices();
    bool getLatestFrame(QImage &outFrame);

signals:
    void errorOccurred(const QString &message);

protected:
    void run() override;

private:
    struct MmapBuffer {
        void  *start  = nullptr;
        size_t length = 0;
    };

    bool openDevice();
    bool initFormat();
    bool initMmap();
    bool startStreaming();
    void stopStreaming();
    void closeDevice();

    QString              m_device;
    int                  m_width = 640;
    int                  m_height = 480;
    int                  m_fd = -1;
    std::vector<MmapBuffer> m_buffers;
    std::atomic<bool>    m_running{false};

    QImage               m_latestFrame;
    std::mutex           m_frameMutex;
    std::atomic<bool>    m_hasNewFrame{false};

    static constexpr int BUFFER_COUNT = 4;
};
