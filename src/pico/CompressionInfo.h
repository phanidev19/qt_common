#ifndef COMPRESSIONINFO_HPP
#define COMPRESSIONINFO_HPP

#include <QByteArray>
#include <QString>

class CompressionInfo
{
public:
    CompressionInfo() {
        this->m_id = -1;
    }

    CompressionInfo(int id, const QByteArray & mzDictionary, const QByteArray & intensityDictionary, const QString & property, const QString & version) :
        m_id(id), m_mzDictionary(mzDictionary), m_intensityDictionary(intensityDictionary), m_property(property), m_version(version)
    {
    }

    void set(int id, const QByteArray & mzDictionary, const QByteArray & intensityDictionary, const QString & property, const QString & version) {
        this->m_id = id;
        this->m_mzDictionary = mzDictionary;
        this->m_intensityDictionary = intensityDictionary;
        this->m_property = property;
        this->m_version = version;
    }

    void clear() {
        m_id = -1;
        m_mzDictionary.clear();
        m_intensityDictionary.clear();
        m_property.clear();
        m_version.clear();
    }

    int GetId() const { return this->m_id; }
    const QByteArray &GetMzDictionary() const { return this->m_mzDictionary; }
    const QByteArray &GetIntensityDictionary() const { return this->m_intensityDictionary; }
    QString GetProperty() const { return this->m_property; }
    QString GetVersion() const { return this->m_version; }

private:
    int m_id;
    QByteArray m_mzDictionary;
    QByteArray m_intensityDictionary;
    QString m_property;
    QString m_version;
};

#endif // COMPRESSIONINFO_HPP
