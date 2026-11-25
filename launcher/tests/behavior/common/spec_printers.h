#ifndef LOM_LAUNCHER_SPEC_PRINTERS_H
#define LOM_LAUNCHER_SPEC_PRINTERS_H

#include <QString>
#include <QStringList>
#include <QUrl>

#include <ostream>

/**
 * @brief Print a string in a failed expectation.
 * @param value String.
 * @param out Output stream.
 */
inline void PrintTo(const QString& value, std::ostream* out) {
    *out << '"' << value.toStdString() << '"';
}

/**
 * @brief Print a string list in a failed expectation.
 * @param value Strings.
 * @param out Output stream.
 */
inline void PrintTo(const QStringList& value, std::ostream* out) {
    *out << '[';

    for (int i = 0; i < value.size(); i++) {
        *out << (i == 0 ? "" : ", ") << '"' << value[i].toStdString() << '"';
    }

    *out << ']';
}

/**
 * @brief Print a URL in a failed expectation.
 * @param value URL.
 * @param out Output stream.
 */
inline void PrintTo(const QUrl& value, std::ostream* out) {
    *out << value.toString().toStdString();
}

#endif
