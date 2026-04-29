#include <QtTest/QtTest>

#include <QFile>

#include <cstddef>

class SourceEncodingGovernanceTest : public QObject
{
    Q_OBJECT

private slots:
    void registration_service_impl_header_is_strict_utf8();

private:
    bool isStrictUtf8(const QByteArray& bytes) const;
    QByteArray readBytes(const QString& relativePath) const;
};

bool SourceEncodingGovernanceTest::isStrictUtf8(const QByteArray& bytes) const
{
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.constData());
    const qsizetype size = bytes.size();
    qsizetype index = 0;

    while (index < size) {
        const unsigned char byte = data[index];
        if (byte <= 0x7F) {
            ++index;
            continue;
        }

        qsizetype sequenceLength = 0;
        uint codePoint = 0;
        if (byte >= 0xC2 && byte <= 0xDF) {
            sequenceLength = 2;
            codePoint = byte & 0x1F;
        } else if (byte >= 0xE0 && byte <= 0xEF) {
            sequenceLength = 3;
            codePoint = byte & 0x0F;
        } else if (byte >= 0xF0 && byte <= 0xF4) {
            sequenceLength = 4;
            codePoint = byte & 0x07;
        } else {
            return false;
        }

        if (index + sequenceLength > size) {
            return false;
        }

        for (qsizetype offset = 1; offset < sequenceLength; ++offset) {
            const unsigned char continuation = data[index + offset];
            if ((continuation & 0xC0) != 0x80) {
                return false;
            }

            codePoint = (codePoint << 6) | (continuation & 0x3F);
        }

        if ((sequenceLength == 2 && codePoint < 0x80)
            || (sequenceLength == 3 && codePoint < 0x800)
            || (sequenceLength == 4 && codePoint < 0x10000)) {
            return false;
        }

        if (codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
            return false;
        }

        index += sequenceLength;
    }

    return true;
}

QByteArray SourceEncodingGovernanceTest::readBytes(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return file.readAll();
}

void SourceEncodingGovernanceTest::registration_service_impl_header_is_strict_utf8()
{
    const QByteArray bytes = readBytes(QStringLiteral("Plugins/RegistrationCore/RegistrationServiceImpl.h"));
    QVERIFY2(isStrictUtf8(bytes),
        "RegistrationServiceImpl.h must be strict UTF-8 without invalid source bytes");
    QVERIFY(!bytes.isEmpty());
}

QTEST_APPLESS_MAIN(SourceEncodingGovernanceTest)
#include "SourceEncodingGovernanceTest.moc"
