#include "window/ui_text.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

#include <string>
#include <unordered_map>

namespace lom::launcher::ui_text {

namespace {

/** Every text, filled in as they're declared. */
std::vector<const Text*>& registry() {
    static std::vector<const Text*> texts;
    return texts;
}

QString current_language = QStringLiteral("en");
std::unordered_map<std::string, QString> current_texts;

/**
 * @brief Read a translation file from the launcher's resources.
 * @param code Language code.
 * @return Its object, empty if there's none.
 */
QJsonObject read_translation(const QString& code) {
    QFile file(QStringLiteral(":/launcher/translations/%1.json").arg(code));

    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }

    return QJsonDocument::fromJson(file.readAll()).object();
}

}

Text::Text(const char* key, const char* english) : key_(key), english_(english) {
    registry().push_back(this);
}

Text::operator QString() const {
    const auto found = current_texts.find(key_);
    return found != current_texts.end() ? found->second : QString::fromUtf8(english_);
}

const std::vector<const Text*>& all() {
    return registry();
}

const std::vector<Language>& languages() {
    static const std::vector<Language> list = {
        {"en", "English"},
        {"de", "Deutsch"},
        {"fr", "Fran\xc3\xa7"
               "ais"},
        {"es", "Espa\xc3\xb1ol"},
        {"ja", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"},
        {"ko", "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4"},
        {"zh_CN", "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87"},
        {"zh_TW", "\xe7\xb9\x81\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87"},
    };
    return list;
}

QString language_for(const QStringList& preferred) {
    for (const QString& entry : preferred) {
        const QStringList parts = QString(entry).replace(QLatin1Char('_'), QLatin1Char('-')).split(QLatin1Char('-'));
        const QString code = parts.value(0).toLower();

        if (code == QLatin1String("zh")) {
            for (const QString& part : parts.mid(1)) {
                const QString tag = part.toLower();

                if (tag == QLatin1String("hant") || tag == QLatin1String("tw") || tag == QLatin1String("hk") || tag == QLatin1String("mo")) {
                    return QStringLiteral("zh_TW");
                }
            }

            return QStringLiteral("zh_CN");
        }

        for (const Language& language : languages()) {
            if (code == QLatin1String(language.code)) {
                return code;
            }
        }
    }

    return QStringLiteral("en");
}

QString system_language() {
    return language_for(QLocale().uiLanguages());
}

void set_language(const QString& code) {
    QString chosen = code.isEmpty() ? system_language() : code;
    bool known = false;

    for (const Language& language : languages()) {
        known = known || chosen == QLatin1String(language.code);
    }

    current_language = known ? chosen : QStringLiteral("en");
    current_texts.clear();

    if (current_language == QLatin1String("en")) {
        return;
    }

    const QJsonObject object = read_translation(current_language);

    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.value().isString() && !it.value().toString().isEmpty()) {
            current_texts[it.key().toStdString()] = it.value().toString();
        }
    }
}

QString language() {
    return current_language;
}

std::vector<std::pair<QString, QString>> translation(const QString& code) {
    std::vector<std::pair<QString, QString>> texts;
    const QJsonObject object = read_translation(code);

    for (auto it = object.begin(); it != object.end(); ++it) {
        texts.emplace_back(it.key(), it.value().toString());
    }

    return texts;
}

}
