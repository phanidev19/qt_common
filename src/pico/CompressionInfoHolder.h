#ifndef COMPRESSIONINFOHOLDER_H
#define COMPRESSIONINFOHOLDER_H

#include <QList>
#include "CompressionInfo.h"

_PMI_BEGIN

class CompressionInfoHolder
{
public:
    CompressionInfoHolder(){

    }

    void addCompressionInfo(const CompressionInfo & compressionInfo) {
        m_compressionInfoList.append(compressionInfo);
    }

    void addCompressionInfo(int id, QByteArray mzDictionary, QByteArray intensityDictionary, QString property, QString version) {
        CompressionInfo compressionInfo(id, mzDictionary, intensityDictionary, property, version);
        m_compressionInfoList.append(compressionInfo);
    }

    void clearList() {
        m_compressionInfoList.clear();
    }

    int getSize() {
        return m_compressionInfoList.size();
    }

    //Note: If we were using FilesId, this function would need to generalize to find filesId as well, right?
    const CompressionInfo* findById(int id) const {
        foreach(const CompressionInfo & compressionInfo, m_compressionInfoList) {
            if(compressionInfo.GetId() == id) {
                return &compressionInfo;
            }
        }

        return NULL;
    }

private:
    QList<CompressionInfo> m_compressionInfoList;

};

_PMI_END

#endif // COMPRESSIONINFOHOLDER_H
