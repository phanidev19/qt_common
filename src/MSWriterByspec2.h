/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 *  Author  : Victor Suponev vsuponev@milosolutions.com
 */

#ifndef MSWRITERBYSPEC2_H
#define MSWRITERBYSPEC2_H

#include "Centroid.h"
#include "Byspec2Types.h"
#include "MSReaderTypes.h"

#include "common_errors.h"

#include <QtSqlUtils.h>

_PMI_BEGIN

/*!
 * \brief The MSWriterByspec2 class provides API for creating and writing byspec2 files.
 *
 * All Err-based methods return kSQLiteExecError, if any SQL error occurred.
 * They returns kError, if the database is not valid.
 * They return kNoErr, if the operation was successful.
 *
 * This class aggregates two different approaches of populating byspec2 files.
 * First utilizes \a makeByspec2, it's based on MSReader and converts a provided format to byspec2.
 * Second is used to populate the byspec2 tables out of raw data structures. It exploits write* methods.
 */
class PMI_COMMON_MS_EXPORT MSWriterByspec2
{
public:
    enum PeakOption {
        Normal = 0x01, //!< Normal MS data will be saved to the Peaks table.
        Centroided = 0x02, //!< Centroided MS data will be saved to the Peaks_MS1Centroided table.
        All = Normal | Centroided, //!< Both normal and centroided data will be saved.
        CentroidedAsNormal = 0x04, //!< Centroided data will be saved to the Peaks table (for Byonic).
        CalculateIntensitySum = 0x08 //!< Intensity sum will be calculated.
    };
    Q_DECLARE_FLAGS(PeakOptions, PeakOption)

    /*!
     * \brief Constructs a MSWriterByspec2 object and initializes the db file with the given
     * \a outputFile, if provided. If empty string is passed, the database is not initialized.
     */
    MSWriterByspec2(const QString &outputFile = QString());

    /*!
     * \brief Destroys the MSWriterByspec2 object and closes the database if open.
     */
    ~MSWriterByspec2();

    /*!
     * \brief Converts the provided \a sourceFile to the byspec2 format and saves the result to
     * \a outputFile. If \a centroided is set to true, MS centroiding is applied and saved to Peaks
     * table. Profile data is not saved. The data from \a argumentList is passed to the FilesInfo
     * table. The \a options param is ignored for now.
     */
    Err makeByspec2(const QString &sourceFile, const QString &outputFile,
                    const msreader::MSConvertOption &options, bool onlyCentroided,
                    const msreader::IonMobilityOptions &ionMobilityOptions,
                    const QStringList &argumentList);

    /*!
     * \brief Creates all tables required by the bysec2 format.
     */
    Err createByspec2Tables();

    /*!
     * \brief Returns true if the database is valid, false otherwise.
     */
    bool isValid();

    /*!
     * \brief Writes the provided \a chromatogram to the database.
     */
    Err writeChromatogram(const byspec2::ChromatogramDaoEntry &chromatogram);

    /*!
     * \brief Writes the provided \a peak to the database. The provided \a options are used to
     * control the peak processing. All | CalculateIntensitySum is the mostly expected \a options
     * value. You can also specify the options for centroiding via \a centroidOptions.
     */
    Err writePeak(const byspec2::PeakDaoEntry &peak, PeakOptions options,
                  const pico::CentroidOptions &centroidOptions = pico::CentroidOptions());

    /*!
     * \brief Writes the provided \a spectrum to the database.
     */
    Err writeSpectrum(const byspec2::SpectrumDaoEntry &spectrum);

    /*!
     * \brief Writes the provided \a compressionInfo to the database.
     */
    Err writeCompressionInfo(const byspec2::CompressionInfoDaoEntry &compressionInfo);

    /*!
     * \brief Writes the provided \a file to the database.
     */
    Err writeFile(const byspec2::FileDaoEntry &file);

    /*!
     * \brief Writes the provided \a fileInfo to the database.
     */
    Err writeFileInfo(const byspec2::FileInfoDaoEntry &fileInfo);

    /*!
     * \brief Writes the provided \a info to the database.
     */
    Err writeInfo(const byspec2::InfoDaoEntry &info);

private:
    /*!
     * \brief Opens the database for the given \a ouputFile.
     */
    Err openDatabase(const QString &outputFile);

    /*!
     * \brief Writes the provided \a peak to the database. The \a isCentroided flag specifies whether
     * the peak will be saved to the Peaks (false) or Peaks_MS1Centroided (true) table.
     */
    Err writePeakToDb(const byspec2::PeakDaoEntry &peak, bool isCentroided = false);

    /*!
     * \brief Saves the given \a sourceFile string to the Files table. The \a fileId out-param is
     * populated with the id of the created entry.
     */
    Err insertDataIntoFilesTable(const QString &sourceFile, int *fileId);
    
    /*!
     * \brief Saves the given file info \a data to the FilesInfo table.
     * The \a fileId specifies the File Id value for the created entry.
     */
    Err insertDataIntoFileInfoTable(const QMap<QString, QString> &data, int fileId);
    
    /*!
     * The \a fileId specifies the File Id value for the created entry.
     * \brief Populates the Peaks and Spectra tables. The MSReader instance must be initialized.
     */
    Err insertDataIntoPeaksAndSpectraTable(bool centroided, int fileId);

    void appendArgumentsToMap(const QStringList &arguments, QMap<QString, QString> &destination);
    void appendIonMobilityOptionsToMap(const msreader::IonMobilityOptions &ionMobilityOptions,
                                        QMap<QString, QString> &destination);

    QSqlDatabase m_database;
    QString m_outputFile;

    static QAtomicInt connCounter;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MSWriterByspec2::PeakOptions)

_PMI_END

#endif // MSWRITERBYSPEC2_H
