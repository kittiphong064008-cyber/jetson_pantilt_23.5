#include "V4L2Camera.h"

#include <QDir>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int xioctl(int fd, unsigned long request, void *arg) {
    int r;
    do { r = ioctl(fd, request, arg); }
    while (r == -1 && errno == EINTR);
    return r;
}

V4L2Camera::V4L2Camera(const QString &device, int width, int height, QObject *parent)
    : QThread(parent), m_device(device), m_width(width), m_height(height) {}

V4L2Camera::~V4L2Camera() {
    stopCapture();
    wait();
}

void V4L2Camera::stopCapture() {
    m_running = false;
}

bool V4L2Camera::getLatestFrame(QImage &outFrame) {
    if (!m_hasNewFrame.load(std::memory_order_relaxed)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_frameMutex);
    outFrame = m_latestFrame;
    m_hasNewFrame.store(false, std::memory_order_relaxed);
    return !outFrame.isNull();
}

QStringList V4L2Camera::availableDevices() {
    QStringList result;
    QDir devDir("/dev");
    const QStringList entries = devDir.entryList(QStringList() << "video*",
                                                  QDir::System, QDir::Name);
    for (const QString &name : entries) {
        QString path = "/dev/" + name;
        int fd = open(path.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
        if (fd < 0) continue;

        v4l2_capability cap{};
        if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
            if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
                (cap.capabilities & V4L2_CAP_STREAMING)) {
                result.append(path);
            }
        }
        close(fd);
    }
    return result;
}

bool V4L2Camera::openDevice() {
    m_fd = open(m_device.toLocal8Bit().constData(), O_RDWR);
    if (m_fd < 0) {
        emit errorOccurred(
            QString("Cannot open %1: %2").arg(m_device, strerror(errno)));
        return false;
    }

    v4l2_capability cap{};
    if (xioctl(m_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        emit errorOccurred(QString("%1: QUERYCAP failed").arg(m_device));
        closeDevice();
        return false;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(cap.capabilities & V4L2_CAP_STREAMING)) {
        emit errorOccurred(
            QString("%1: not a streaming capture device").arg(m_device));
        closeDevice();
        return false;
    }
    return true;
}

bool V4L2Camera::initFormat() {
    v4l2_format fmt{};
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = m_width;
    fmt.fmt.pix.height      = m_height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    if (xioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0) {
        emit errorOccurred(
            QString("%1: cannot set MJPEG %2x%3")
                .arg(m_device)
                .arg(m_width)
                .arg(m_height));
        return false;
    }

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
        emit errorOccurred(
            QString("%1: driver did not accept MJPEG").arg(m_device));
        return false;
    }
    return true;
}

bool V4L2Camera::initMmap() {
    v4l2_requestbuffers req{};
    req.count  = BUFFER_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(m_fd, VIDIOC_REQBUFS, &req) < 0) {
        emit errorOccurred(QString("%1: REQBUFS failed").arg(m_device));
        return false;
    }

    m_buffers.resize(req.count);

    for (size_t i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (xioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            emit errorOccurred(
                QString("%1: QUERYBUF %2 failed").arg(m_device).arg(i));
            return false;
        }

        m_buffers[i].length = buf.length;
        m_buffers[i].start  = mmap(nullptr, buf.length,
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED, m_fd, buf.m.offset);

        if (m_buffers[i].start == MAP_FAILED) {
            emit errorOccurred(
                QString("%1: mmap %2 failed").arg(m_device).arg(i));
            return false;
        }
    }
    return true;
}

bool V4L2Camera::startStreaming() {
    for (size_t i = 0; i < m_buffers.size(); ++i) {
        v4l2_buffer buf{};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (xioctl(m_fd, VIDIOC_QBUF, &buf) < 0) {
            emit errorOccurred(
                QString("%1: QBUF %2 failed").arg(m_device).arg(i));
            return false;
        }
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(m_fd, VIDIOC_STREAMON, &type) < 0) {
        emit errorOccurred(
            QString("%1: STREAMON failed").arg(m_device));
        return false;
    }
    return true;
}

void V4L2Camera::stopStreaming() {
    if (m_fd < 0) return;
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(m_fd, VIDIOC_STREAMOFF, &type);
}

void V4L2Camera::closeDevice() {
    stopStreaming();
    for (auto &b : m_buffers) {
        if (b.start && b.start != MAP_FAILED)
            munmap(b.start, b.length);
    }
    m_buffers.clear();
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

void V4L2Camera::run() {
    if (!openDevice() || !initFormat() || !initMmap() || !startStreaming()) {
        closeDevice();
        return;
    }

    m_running = true;

    while (m_running) {
        struct pollfd pfd{};
        pfd.fd     = m_fd;
        pfd.events = POLLIN;
        int ret = poll(&pfd, 1, 100);
        if (ret <= 0) continue;

        v4l2_buffer buf{};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(m_fd, VIDIOC_DQBUF, &buf) < 0) {
            if (errno == EAGAIN) continue;
            break;
        }

        QImage frame;
        if (frame.loadFromData(
                static_cast<const uchar *>(m_buffers[buf.index].start),
                static_cast<int>(buf.bytesused), "JPEG")) {
            std::lock_guard<std::mutex> lock(m_frameMutex);
            m_latestFrame = std::move(frame);
            m_hasNewFrame.store(true, std::memory_order_relaxed);
        }

        if (xioctl(m_fd, VIDIOC_QBUF, &buf) < 0) break;
    }

    closeDevice();
}
