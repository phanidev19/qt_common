/*
* Copyright (C) 2015 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Yong Joo Kil ykil@proteinmetrics.com
*/

#ifndef __AVERAGINE_COMPARISON_H__
#define __AVERAGINE_COMPARISON_H__

#include "pmi_common_core_mini_export.h"
#include <PlotBase.h>

#include <QString>
#include <QVector>

#include <vector>

_PMI_BEGIN

namespace coreMini {

//THIS VERSION WAS EXTRACTED FROM byonic_solutions: feature/create_isotope_distribution_from_peptide, 48917d1eccdc
//THIS VERSION WILL EVENTUALLY replace the current AveragineComparion.h/cpp.  For now, use this code for its
//SpecificIsotopeDistribution class.

struct PMI_COMMON_CORE_MINI_EXPORT ProcessorOptions
{
    int po_SpectraIdStart;
    int po_SpectraIdEnd;

    double po_PrecursorWindowLeftWidth;
    double po_PrecursorWindowRightWidth;

    int po_NumberMonosReturn;

    double po_MinIonCurrentRatio;

    double po_uncertainty;
    UncertaintyScaling po_uncertainty_scaling_type;
    int po_MinZCheck;
    int po_MaxZCheck;

    ProcessorOptions() {
        po_SpectraIdStart = -1;
        po_SpectraIdEnd = -1;

        po_PrecursorWindowLeftWidth = 1.5;
        po_PrecursorWindowRightWidth = 1.5;

        po_NumberMonosReturn = 1;

        po_MinIonCurrentRatio = .1;

        po_uncertainty = 0.015; // uncertainty for 1000Da molecule
        po_uncertainty_scaling_type = SquareRootUncertainty; // this is dangerous
        po_MinZCheck = 1;
        po_MaxZCheck = 35;
    }
};

struct PMI_COMMON_CORE_MINI_EXPORT PointAndPercentAccountFor
{
    int index;
    double percent_account_for;
    PointAndPercentAccountFor() {
        index = -1;
        percent_account_for = 0;
    }
};

struct PMI_COMMON_CORE_MINI_EXPORT ChargeMonosCandidate
{
    int charge;
    double mono;
    ChargeMonosCandidate() {
        charge = 0;
        mono = 0;
    }
    ChargeMonosCandidate(int xcharge, double xmono) {
        charge = xcharge;
        mono = xmono;
    }
};

struct TestParameters;

struct PMI_COMMON_CORE_MINI_EXPORT ChargeScoreInfo
{
    int charge; //! isotope charge
    double score; //! overlap score
    double mzOffset; //! mono mz position
    double scaleFact; //! factor difference between average peak scaled to sum = 1
                      //! to corresponding spectrum peak
    std::vector<PointAndPercentAccountFor> matchVector; //! indicies of peaks matches to theoretical peaks
    std::vector<point2d> matchPointsVector;
    double percent_ion_current;

    ChargeScoreInfo() {
        clear();
    }

    void clear() {
        charge = 0;
        score = 0;
        mzOffset = 0;
        scaleFact = 0;
        matchVector.clear();
        matchPointsVector.resize(10);
        matchPointsVector.clear();
        matchVector.resize(10);
        percent_ion_current = 0;
    }

    ChargeScoreInfo(int xcharge, double xscore, double xmzOffset, double xscaleFact,
                    const std::vector<PointAndPercentAccountFor> & xmatchVector)
        : charge(xcharge), score(xscore), mzOffset(xmzOffset), scaleFact(xscaleFact), matchVector(xmatchVector)
    {
        percent_ion_current = 0;
    }

    double getPointsIntensBetweenIndices(TestParameters &test_parameters, int window_start, int window_end) const;
    double getPointsIntensBetweenIndices(int window_start, int window_end) const;

    double getPointsIntes() const {
        double intensity = 0;
        for (int mpv_index = 0; mpv_index < (int)matchPointsVector.size(); mpv_index++) {
            point2d matchPoint = matchPointsVector[mpv_index];
            double percent_accounted_for = matchVector[mpv_index].percent_account_for;
            if (percent_accounted_for < 1) {
                intensity += matchPoint.y() * percent_accounted_for;
            } else {
                intensity += matchPoint.y();
            }
        }
        return intensity;
    }

    double getScore() const {
        return score;
    }
    double getMzOffset() const {
        return mzOffset;
    }
    double getCharge() const {
        return charge;
    }
    double getScaleFact() const{
        return scaleFact;
    }
    double getPercentIonCurrent() const {
        return percent_ion_current;
    }
    std::vector<PointAndPercentAccountFor> getMatchVector() const {
        return matchVector;
    }
};

struct PMI_COMMON_CORE_MINI_EXPORT DebugStruct
{
    std::vector<point2d> xcorrValues2dList_debug;
    std::vector<point2d> testValues2dList_debug;
    DebugStruct() {
        clear();
    }
    void clear() {
        xcorrValues2dList_debug.clear();
        testValues2dList_debug.clear();
    }
};

struct PMI_COMMON_CORE_MINI_EXPORT amino_atomic_dist
{
    char letter[10]; ///Note: longest letter can be 9 letters plus nil.
    int atomic_dist[5];
};

struct PMI_COMMON_CORE_MINI_EXPORT IsotopeWorkspace
{
    enum IsotopeCompositionRatios {
         IsotopeCompositionRatios_ByonicValuesThusFar_2015_01_26
        ,IsotopeCompositionRatios_ProteinProspector
        ,IsotopeCompositionRatios_Xcalibur
    };

    enum IsotopeComposition {
        FirstElement = 0
       ,Carbon = FirstElement
       ,Hydrogen
       ,Nitrogen
       ,Oxygen
       ,Sulfur
       ,IsotopeCompositionLength
    };

    std::vector<double> averagComp;
    std::vector< std::vector<double> > isotopeCompList;

    std::vector<double> cVector;
    std::vector<double> hVector;
    std::vector<double> nVector;
    std::vector<double> oVector;
    std::vector<double> sVector;

    double massScaleFact;
    std::map<std::string, int*> amino_acid_atomic_compositions;

    explicit IsotopeWorkspace(IsotopeCompositionRatios type);
};

class PMI_COMMON_CORE_MINI_EXPORT SpecificIsotopeDistribution {
public:
    SpecificIsotopeDistribution(const std::string &amino_acid_sequence, bool add_water, int charge, double interIsoTrim);

    ~SpecificIsotopeDistribution() {
    }

    const std::vector<double> & getIsoList() const;
    void PrintOutIsoList();

private:
    void AddElementsToSequenceElements(IsotopeWorkspace & isoWork, const std::vector<int> & numberOfElements,
                                       std::vector<double> &isotopesList);
    std::vector<double> m_isotopeDist;
};

class PMI_COMMON_CORE_MINI_EXPORT AveragineIsotopeTable {
public:
  struct PMI_COMMON_CORE_MINI_EXPORT Parameters {
    /// (ie ->1000*100 checks masses up to 100,000Da)
    double numberAveragDists = 1000;

    /// (ie 1000*->100 checks masses up to 100,000Da)
    double massSpacing = 100;

    /// intensity ratio of smallest isotope to total sum
    /// kept during construction of distribution
    double interIsoTrim = 0.0001;

    /// intensity ratio of smallest isotope to
    /// total sum kept in final distribution
    double finalIsoTrim = 0.001;

    /*!
     * \brief make dev friendly string; not expected to be parsable.
     * \return
     */
    QString toStringDevFriendly() const;

    /*!
     * \brief These are the parameters deemed to be accurate enough for
     * computing the proxy Total XIC AUC. Note: these magic numbers were chosen
     * based on some experiments. These made sure all table entries summed to 1.
     * We may be able to optimize these values if there are speed or memory
     * issues.
     *
     * Reference:
     * https://proteinmetrics.atlassian.net/browse/LT-2201?focusedCommentId=63064&page=com.atlassian.jira.plugin.system.issuetabpanels%3Acomment-tabpanel#comment-63064
     *
     * \return parameter
     */
    static Parameters makeDefaultForAccurate();

    /*!
     * \brief These defaults used are the ones we've set to be used for original
     * Byomap feature finder. Note that these values do not sum to 1 (can fall
     * down to 0.8 at higher mass values). But we will keep them to preserve
     * backwards compatability.
     *
     * \return parameter
     */
    static Parameters makeDefaultForByomapFeatureFinder();
  };

  AveragineIsotopeTable();

  AveragineIsotopeTable(const Parameters &parameter);

  ~AveragineIsotopeTable() {}

  /*!
   * \brief isotopeIntensityFractions returns the isotope vector closest to the
   * given mass (via rounding) \param mass mass in Daltons \return vector to the
   * isotope. if mass is out of bounds for cached internal table, it will return
   * either the first (too small or negative) or the last vector (too large,
   * e.g. larger than 100K Dalton).
   */
  const std::vector<double> &isotopeIntensityFractions(double mass) const;

  /*!
   * \brief isotopeFractionInterpolated will provide the isotope ratio value for
   * the given mass and isotope. The sum of all isotopes for the given mass will
   * be equal to one. See LT-2201 for details. \param mass input mass \param
   * isotope input isotope \return normalized intenisity value
   */
  double isotopeFractionInterpolated(double mass, int isotope) const;

  /*!
   * \brief save the internal table to csv
   * \param filename
   */
  void saveToFile(const QString &filename) const;

  std::vector<std::vector<double>> averagineTable() const;

private:
  /*!
   * \brief isotopeIntensityFractionsIndexRound returns the index to the best
   * isotope vector, for the given mass. This is mostly here to keep backwards
   * compatability with existing function of isotopeIntensityFractions \param
   * mass \return index to the clostest isotope vector
   */
  int isotopeIntensityFractionsIndexRound(double mass) const;

  /*!
   * \brief isotopeIntensityFractionsIndexFloor returns the index to the floor
   * of \param mass \param alpha \return
   */
  int isotopeIntensityFractionsIndexFloor(double mass, double *alpha) const;

  void construstAveragineIsotopes(IsotopeWorkspace &isoWork);

  void constructIsotopeList(const IsotopeWorkspace &isoWork,
                            std::vector<int> &numberOfNewElements,
                            std::vector<double> &averagIsoPeaks);

private:
  std::vector<std::vector<double>> m_averagTable;
  Parameters m_parameters;
};

PMI_COMMON_CORE_MINI_EXPORT double findStdDev(double stdDev, double mzOffset, UncertaintyScaling uncertainty_scaling_type);

class PMI_COMMON_CORE_MINI_EXPORT AveragineIsotopeFunction {
public:
    explicit AveragineIsotopeFunction(const std::vector<double> & isoDist);
    ~AveragineIsotopeFunction() {
    }

    void setCharge(int charge) {
        m_charge = charge;
    }
    // the standard deviation value must be assigned after a valid mzOffset is assigned
    void setStdDev(double stdDev, UncertaintyScaling uncertainty_scaling_type) {
        m_stdDev = findStdDev(stdDev, m_mzOffset, uncertainty_scaling_type);
    }
    void setMzOffset(double mzOffset) {
        m_mzOffset = mzOffset;
    }
    void setScaleFact(double scaleFact) {
        m_scaleFact = scaleFact;
    }

    double mzOffset() const {
        return m_mzOffset;
    }
    int charge() const {
        return m_charge;
    }
    double stdDev() const {
        return m_stdDev;
    }
    double scaleFact() const {
        return m_scaleFact;
    }
    const std::vector<double> & averagIsoDist() const {
        return m_averagIsoDist;
    }

    double evaluate(double mz, int clamping) const;
    double evaluateDeriv(double mz) const;
    int evaluateSecondDerivSign(double mz) const;
    int peakOverlapIndex(double mz) const;
    const std::vector<double> & getDistributionList() const {
        return m_averagIsoDist;
    }
    Err saveToFile(double mz_start, double mz_end, double increment,
                   QString filename) const;

private:
    const std::vector<double> & m_averagIsoDist;
    int m_charge;
    double m_mzOffset;
    double m_stdDev;
    double m_scaleFact;
};

struct PMI_COMMON_CORE_MINI_EXPORT PeakTag
{
    double xVal;
    double yVal;
    bool excluded;
    int x_index; // index to the window space point
    //TODO: bool is_excluded() const;

    PeakTag(double xXVal, double xYVal, bool xExcluded, int _x_index) {
        xVal = xXVal;
        yVal = xYVal;
        excluded = xExcluded;
        x_index = _x_index;
    }
    PeakTag(){
        xVal = -1;
        yVal = -1;
        excluded = true;
        x_index = -1;
    }
};

class PMI_COMMON_CORE_MINI_EXPORT PeakTagList
{
public:
    PeakTagList();

    ~PeakTagList() {
    }

    point2d getWindowPoint(int pointToCheckIndx);
    point2d getWindowPointSorted(int pointToGetIndx);
    void excludePoint(int excludedPointIndx);
    bool checkPointExcludedSorted(int pointToCheckIndx);
    bool checkPointExcludedUnsorted( int pointToCheckIndx);
    int  getSize() const;
    bool checkAllPointsExcluded();
    void addPoints2dList(const point2dList & specPointsList);
    void addPoints2dList_v2(const point2dList & specPointsList);

    void clear() {
        m_windowPoints.clear();
        m_windowPointsIndices.clear();
        m_window_peak_offset_from_specpoints = 0; //yong: 2015-08-05
    }

    void setWindowPeakOffset(int window_peak_offset);
    int getWindowPeakOffsetFromSpecpoints();

private:
    QVector<PeakTag> m_windowPoints;  //! sorted by y-value, descending
    QVector<int> m_windowPointsIndices; //! sorted by x-value, ascending
    int m_window_peak_offset_from_specpoints;
};

struct PMI_COMMON_CORE_MINI_EXPORT TestParameters
{
    PeakTagList current_match_list;
    bool charge_check;
    bool scaling_check;
    int charge_start;
    int charge_end;
    bool score_version_1;
    UncertaintyScaling uncertainty_scaling_type;
    double uncertainty;
    bool Byonic;

    TestParameters() {
        charge_check = false;
        scaling_check = false;
        charge_start = 1;
        charge_end = 35;
        score_version_1 = false;
        uncertainty_scaling_type = LinearUncertainty;
        uncertainty = 0;
        Byonic = false;
    }
};

PMI_COMMON_CORE_MINI_EXPORT bool monoMzLess(const ChargeScoreInfo & mono1, const ChargeScoreInfo & mono2);
PMI_COMMON_CORE_MINI_EXPORT bool chargeLess(const ChargeScoreInfo & mono1, const ChargeScoreInfo & mono2);
PMI_COMMON_CORE_MINI_EXPORT bool scoreLess(const ChargeScoreInfo & mono1, const ChargeScoreInfo & mono2);
PMI_COMMON_CORE_MINI_EXPORT bool intensGreater(const ChargeScoreInfo & mono1, const ChargeScoreInfo & mono2);
PMI_COMMON_CORE_MINI_EXPORT bool areUnique(const TestParameters & test_parameters, const ChargeScoreInfo & mono1, const ChargeScoreInfo & mono2);

PMI_COMMON_CORE_MINI_EXPORT int findMinIndex(const std::vector<double> & list);
PMI_COMMON_CORE_MINI_EXPORT int findMaxIndex(const std::vector<double> & list);

PMI_COMMON_CORE_MINI_EXPORT void findChargesAndMonos(const ProcessorOptions& opts, const PlotBase & ms1Centroided,
                         double specWindowMZ_start, double specWindowMZ_end, QString ms1ScanNumberStr,
                         double percent_ion_current_allow, QVector<ChargeScoreInfo> & bestMonosList);

//void
//findChargeScoreInfo(const ProcessorOptions& opts, PlotBase & ms1Centroided,
//                    QList<ChargeScoreInfo> & chargeMonosCandList);\

PMI_COMMON_CORE_MINI_EXPORT void testMonoInitial(const PlotBase & pointsList, double stdDev, ChargeScoreInfo & overlapObject);

PMI_COMMON_CORE_MINI_EXPORT void testMonoFinal(const PlotBase & pointsList, double stdDev, ChargeScoreInfo & overlapObject);

PMI_COMMON_CORE_MINI_EXPORT void sampleAveragine(AveragineIsotopeFunction & isoFunc, double start_mz,
                                            double end_mz, double stepSize, point2dList & averagineSampling);

PMI_COMMON_CORE_MINI_EXPORT QString makeMonoChargeYScaleStdDevToString(double mass_mono_uncharged,
                                                                  int charge, double y_scale, double stdDev);

PMI_COMMON_CORE_MINI_EXPORT void makeMonoChargeYScaleStdDevFromString(const QString & str, double & mass_mono_uncharged,
                                          int & charge, double & y_scale, double & std_dev);

PMI_COMMON_CORE_MINI_EXPORT void findBestMonos(const PlotBase & ms1Centroided, double specWindowMZ_start,
                                          double specWindowMZ_end, const ProcessorOptions & opts,
                                          QVector<ChargeScoreInfo> & bestMonos);

PMI_COMMON_CORE_MINI_EXPORT void searchFinePosition(const PlotBase & ms1Centroided, double leftBoundOff,
                                               double rightBoundOff, double mzStepSize,
                                               TestParameters & test_parameters, ChargeScoreInfo & overlapObject);

PMI_COMMON_CORE_MINI_EXPORT void queryFinePosition(const PlotBase & ms1Centroided, ChargeScoreInfo & overlapObject);

PMI_COMMON_CORE_MINI_EXPORT void searchFineScaleFactor(const PlotBase & ms1Centroided, double start_ratio,
                                                  double end_ratio, double interval,
                                                  TestParameters & test_parameters,
                                                  ChargeScoreInfo & overlapObject);

PMI_COMMON_CORE_MINI_EXPORT void searchFineScaleFactorBinary(const PlotBase & ms1Centroided,
                                                        double start_ratio, double end_ratio, int number_checks,
                                                        TestParameters & test_parameters,
                                                        ChargeScoreInfo & overlapObject);

} // coreMini

_PMI_END


#endif
