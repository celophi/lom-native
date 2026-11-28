#ifndef LOM_LAUNCHER_QT_DOWNLOAD_PROVIDER_H
#define LOM_LAUNCHER_QT_DOWNLOAD_PROVIDER_H

#include "services/download_provider.h"

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

namespace lom::launcher {
class QtDownloadProvider : public QObject, public DownloadProvider {
    Q_OBJECT

  public:
    /**
     * @brief A provider downloading with Qt's network access.
     * @param parent Owner.
     */
    explicit QtDownloadProvider(QObject* parent);

    ~QtDownloadProvider() override;

    void get(const QUrl& url, std::function<void(const QByteArray& piece)> received, std::function<void(qint64 received, qint64 total)> progress,
             std::function<void(const Done& done)> done) override;

    void cancel() override;

  private:
    QNetworkAccessManager* network_ = nullptr;
    QNetworkReply* reply_ = nullptr;
    bool cancelled_ = false;
};

}

#endif
