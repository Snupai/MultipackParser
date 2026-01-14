/**
 * @file UsbKeyCheck.cpp
 * @brief Implementation of USB key authentication check
 */

#include "multipack/system/UsbKeyCheck.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>

namespace multipack {
namespace system {

static const QString DEFAULT_KEY_FILE = ".multipack_key";

UsbKeyCheck::UsbKeyCheck(QObject* parent)
    : QObject(parent)
    , m_keyFileName(DEFAULT_KEY_FILE)
{
    qDebug() << "UsbKeyCheck::UsbKeyCheck - constructor";

    // Add default search paths
#ifdef Q_OS_WIN
    // Windows: Check common USB mount points
    m_searchPaths << "D:/" << "E:/" << "F:/" << "G:/";
#else
    // Linux: Check common USB mount points
    m_searchPaths << "/media/usb0" << "/media/usb1"
                  << "/mnt/usb" << "/media/pi";
#endif
}

UsbKeyCheck::~UsbKeyCheck()
{
    qDebug() << "UsbKeyCheck::~UsbKeyCheck - destructor";
}

bool UsbKeyCheck::isKeyPresent() const
{
    for (const QString& path : m_searchPaths) {
        if (checkPath(path)) {
            return true;
        }
    }
    return false;
}

bool UsbKeyCheck::checkPath(const QString& path) const
{
    QDir dir(path);
    if (!dir.exists()) {
        return false;
    }

    QString keyPath = dir.filePath(m_keyFileName);
    return verifyKeyFile(keyPath);
}

QString UsbKeyCheck::keyFileName()
{
    return DEFAULT_KEY_FILE;
}

void UsbKeyCheck::setKeyFileName(const QString& name)
{
    m_keyFileName = name;
}

void UsbKeyCheck::addSearchPath(const QString& path)
{
    if (!m_searchPaths.contains(path)) {
        m_searchPaths.append(path);
    }
}

void UsbKeyCheck::clearSearchPaths()
{
    m_searchPaths.clear();
}

QStringList UsbKeyCheck::searchPaths() const
{
    return m_searchPaths;
}

bool UsbKeyCheck::verifyKeyFile(const QString& path) const
{
    QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    // Read first line and verify format
    QByteArray content = file.readLine().trimmed();
    file.close();

    // TODO: Implement proper key verification
    // For now, just check that file exists and is not empty
    if (content.isEmpty()) {
        return false;
    }

    qDebug() << "USB key file found:" << path;
    return true;
}

} // namespace system
} // namespace multipack
