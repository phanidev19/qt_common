#include "MSReaderThermo.h"

_PMI_BEGIN
_MSREADER_BEGIN

MSReaderThermo::MSReaderThermo() :
    m_rawHandle(NULL)
{
    m_modelType = thermo::InstrumentModelType_Unknown;
}

MSReaderThermo::~MSReaderThermo()
{
    if (m_rawHandle) {
        CloseAndDestroyRawInstance(m_rawHandle);
    }
}

void MSReaderThermo::clear()
{
    if (m_rawHandle) {
        CloseAndDestroyRawInstance(m_rawHandle);
        m_rawHandle = NULL;
    }
    m_instModel.clear();
    m_modelType = thermo::InstrumentModelType_Unknown;
    MSReaderBase::clear();
}

MSReaderBase::MSReaderClassType MSReaderThermo::classTypeName() const
{
    return MSReaderClassTypeThermo;
}

bool MSReaderThermo::canOpen(const QString & path) const {
    std::string pathstd = path.toStdString();

    QFileInfo info(path);
    bool file_exists = info.exists();
    bool isfile = info.isFile();
    bool path_raw = path.endsWith(".raw", Qt::CaseInsensitive);

    if (file_exists && isfile && path_raw) {
        return true;
    }

    return false;
}

Err MSReaderThermo::openFile(const QString &fileName, MSConvertOption convert_options,
                             QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    Q_UNUSED(convert_options);
    Q_UNUSED(progress);

//    commonMsDebug() << "openFile start: " << fileName;
//    commonMsDebug() << "m_filename: " << m_filename;
//    commonMsDebug() << "m_rawHandle: " << m_rawHandle;


    //If m_rawHandle exists with the given filename, then no need to reopen handle.
    if (getFilename() == fileName && m_rawHandle) {
        return e;
    }
    clear();

    e = CreateRawInstanceAndOpenFile(fileName.toStdString(), &m_rawHandle, ticControllerType,
                                     ticControllerNumber);
    eee;
    if (m_rawHandle) {
        setFilename(fileName);
        e = getInstrumentModel(m_rawHandle, m_modelType, m_instModel); eee;
        //commonMsDebug() << "instModel=" << m_instModel.c_str();
    } else {
        e = kRawReadingError; eee;
    }

error:
    return e;
}

Err MSReaderThermo::closeFile()
{
    Err e = kNoErr;
    clear();  //Note: this will deallocate m_rawHandle
    return e;
}

//Err MSReaderBase::getFileInfo(QMap<QString,QString> & infoItems) const {
//    Err e = kNoErr;
//    infoItems.clear();
//    if (m_rawHandle == NULL) {
//        e = kRawReadingError; eee;
//    }

//error:
//    return e;
//}

Err MSReaderThermo::getBasePeak(point2dList *points) const
{
    Q_UNUSED(points);

    return kFunctionNotImplemented;
}

Err MSReaderThermo::getTICData(point2dList *points) const
{
    Q_ASSERT(points);

    Err e = kNoErr;
    if (m_rawHandle == NULL) {
        e = kRawReadingError; eee;
    }

    e = getTICPointList_AutoSetCurrentController(m_rawHandle, *points, true); eee;

error:
    return e;
}

Err MSReaderThermo::getScanData(long scanNumber, point2dList *points, bool do_centroiding,
                                PointListAsByteArrays *pointListAsByteArrays)
{
    Q_ASSERT(points);

    Err e = kNoErr;
    Q_UNUSED(pointListAsByteArrays);
    if (m_rawHandle == NULL) {
        e = kRawReadingError; eee;
    }
    //Huh? Dead comment?: "Always defaults to TIC, so no need to set to TIC"

    if (do_centroiding) {
        e = getRawScanNumberToPointListSortByX_centroid(m_rawHandle, m_modelType, scanNumber,
                                                        *points); eee;
    } else {
        e = getRawScanNumberToPointListSortByX(m_rawHandle, scanNumber, *points); eee;
    }

error:
    return e;
}

Err MSReaderThermo::getXICData(const XICWindow &win, point2dList *points, int ms_level) const
{
    Q_ASSERT(points);

    Err e = kNoErr;
    Q_UNUSED(ms_level);
    if (m_rawHandle == NULL) {
        e = kRawReadingError; eee;
    }
    //ms_level: Not implemented for Thermo, but we should.
    e = extractRAWXIC(m_rawHandle, win, *points); eee;

error:
    return e;
}

Err MSReaderThermo::getScanInfo(long scanNumber, ScanInfo *obj) const
{
    Q_ASSERT(obj);

    Err e = kNoErr;
    std::string peakModeStr, scanMethodStr; //precursorMassStr
    obj->peakMode = PeakPickingModeUnknown;
    obj->scanMethod = ScanMethodUnknown;

    if (m_rawHandle == NULL) {
        e = kRawReadingError; eee;
    }

    e = getMSLevel(m_rawHandle, scanNumber, &obj->scanLevel, NULL, &peakModeStr, &scanMethodStr); eee;
    if (peakModeStr == "p") {
        obj->peakMode = PeakPickingProfile;
    } else if (peakModeStr == "c") {
        obj->peakMode = PeakPickingCentroid;
    }
    if (scanMethodStr == "Full")
        obj->scanMethod = ScanMethodFullScan;
    else if (scanMethodStr == "Z")
        obj->scanMethod = ScanMethodZoomScan;

    e = getRT(m_rawHandle, scanNumber, &obj->retTimeMinutes); eee;
    obj->nativeId = makeThermoNativeId(scanNumber);

error:
    return e;
}

Err MSReaderThermo::getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const
{
    Q_ASSERT(pinfo);

    Err e = kNoErr;
    if (m_rawHandle == NULL) {
        e = kRawReadingError; eee;
    }

    e = getPrecursorInfo(m_rawHandle, scanNumber, *pinfo); eee;

error:
    return e;
}

Err MSReaderThermo::getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const
{
    Err e = kNoErr;
    if (m_rawHandle == NULL) {
        e = kRawReadingError; eee;
    }

    e = RAW_getNumberOfSpectra(m_rawHandle, totalNumber, startScan, endScan); eee;

error:
    return e;
}

Err MSReaderThermo::getFragmentType(long scanNumber, long scanLevel, QString *fragmentType) const
{
    Q_ASSERT(fragmentType);

    Err e = kNoErr;
    std::string fragStr;
    fragmentType->clear();

    if (m_rawHandle == NULL) {
        e = kRawReadingError; eee;
    }

    e = RAW_getFragmentType(m_rawHandle, scanNumber, scanLevel, *fragmentType); eee;

error:
    return e;
}

Err MSReaderThermo::getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const
{
    Q_ASSERT(scanNumber);

    Err e = kNoErr;
    int curr_mslevel = -1;
    long curr_scanNumber = -1;
    *scanNumber = -1;

    if (m_rawHandle == NULL) {
        e = kRawReadingError; eee;
    }

    e = getScanNumberFromRT(m_rawHandle, scanTimeMinutes + MSReaderBase::scantimeFindParentFudge,
                            &curr_scanNumber); eee;
    while (curr_scanNumber >= 1) {
        e = getMSLevel(m_rawHandle, curr_scanNumber, &curr_mslevel, NULL, NULL, NULL); eee;
        if (curr_mslevel == msLevel) {
            *scanNumber = curr_scanNumber;
            return e;
        }
        curr_scanNumber--;
    }
    //No MS level found.
    e = kBadParameterError; eee;
error:
    return e;
}

const QString internal_channel_name_template = QString("ctr_t,ctr_n,chnl_n;%1,%2,%3");

static Err extract_from_controller(RawFileHandle rawHandle, ThermoControllerType controller_type,
                                   QString channel_name_template, int channel_start,
                                   int channel_end, QList<ChromatogramInfo> *chroInfoList)
{
    Q_ASSERT(chroInfoList);

    Err e = kNoErr;
    long nControllers = 0;
    ChromatogramInfo chroInfo;

    if (rawHandle == NULL) {
        e = kRawReadingError; eee;
    }

    e = GetNumberOfControllersOfType(rawHandle, controller_type, &nControllers); eee;
    for (long controller_num = 1; controller_num <= nControllers; controller_num++) {
        e = SetRawCurrentController(rawHandle, controller_type, controller_num);  eee;
        for (int channel = channel_start; channel <= channel_end; channel++) {
            chroInfo.clear();
            e = getChroPoints(rawHandle, channel, chroInfo.points, true);
            if (e) {
                // If error, assume that channel does not exists and continue;
                e = kNoErr;
                continue;
            }
            if (chroInfo.points.size() > 0) {
                // note: controller is implied by the "TIC", "UV", etc... string
                chroInfo.channelName = channel_name_template.arg(controller_num).arg(channel);
                chroInfo.internalChannelName = internal_channel_name_template.arg(controller_type)
                                                   .arg(controller_num)
                                                   .arg(channel);
                chroInfoList->push_back(chroInfo);
            }
        }
    }

error:
    return e;
}

/*!
 * \brief This function will eventually generalize getTIC and getUV.
 *        For now, this replace getUVData() but not getTICData()
 * \param chroList
 * \return Err
 */
Err MSReaderThermo::getChromatograms(QList<ChromatogramInfo> *chroInfoList,
                                     const QString &channelInternalName)
{
    Q_ASSERT(chroInfoList);

    Err e = kNoErr;
    chroInfoList->clear();

    if (m_rawHandle == NULL) {
        e = kRawReadingError;
        eee;
    }

    // Note from MSFileReader Reference pdf:
    //"The controller number is used to indicate which device
    // controller to use if there is more than one registered device controller of the same type
    // (for  example, multiple UV detectors). Controller numbers for each type are numbered starting
    // at 1."
    // For now, we will assume the end user has single UV detector, but may have multiple channels.

    if (channelInternalName.size() > 0) {
        // Note: expects string int form of
        // QString("ctr_t,ctr_n,chnl_n;%1,%2,%3").arg(controller_type).arg(controller_num).arg(channel);
        ChromatogramInfo chroInfo;
        QStringList list = channelInternalName.split(";", QString::SkipEmptyParts);
        QStringList args;
        long controller_type, controller_num, channel;
        if (list.size() != 2) {
            e = kBadParameterError; eee;
        }
        args = list[1].split(",");
        if (args.size() != 3) {
            e = kBadParameterError; eee;
        }
        controller_type = args[0].toInt();
        controller_num = args[1].toInt();
        channel = args[2].toInt();

        e = SetRawCurrentController(m_rawHandle, controller_type, controller_num); eee;
        chroInfo.clear();

        // This should not fail since we are explicitly asking for this channel.
        e = getChroPoints(m_rawHandle, channel, chroInfo.points, true); eee;
        
        if (chroInfo.points.size() > 0) {
            // note: controller is implied by the "TIC", "UV", etc... string
            chroInfo.channelName = QString("ControlType=%1, ControlNumber = %2, Channel = %3")
                                       .arg(controller_type)
                                       .arg(controller_num)
                                       .arg(channel);
            chroInfo.internalChannelName = internal_channel_name_template.arg(controller_type)
                                               .arg(controller_num)
                                               .arg(channel);
            chroInfoList->push_back(chroInfo);
        }
    } else {
        e = extract_from_controller(m_rawHandle, ThermoControllerUV,
                                    QString("UV ControlNumber = %1, Channel = %2"),
                                    ThermoUVChannelA, ThermoUVChannelD, chroInfoList); eee;
        e = extract_from_controller(m_rawHandle, ThermoControllerAnalog,
                                    QString("Analog ControlNumber = %1, Channel = %2"),
                                    ThermoAnalog1, ThermoAnalog4, chroInfoList); eee;
        // For now, PDA gets all wavelengths (total scan)
        e = extract_from_controller(m_rawHandle, ThermoControllerPDA,
                                    QString("PDA(total scan) ControlNumber = %1, Channel = %2"),
                                    ThermoPDAChroTotalScan, ThermoPDAChroTotalScan, chroInfoList); eee;
        e = extract_from_controller(m_rawHandle, ThermoControllerADCard,
                                    QString("A/D Card ControlNumber = %1, Channel = %2"),
                                    ThermoADCardCh1, ThermoADCardCh4, chroInfoList); eee;
    }

error:
    // always default to TIC
    SetRawCurrentController(m_rawHandle, ticControllerType, ticControllerNumber);
    return e;
}

_MSREADER_END
_PMI_END
