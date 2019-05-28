/*
* Copyright (C) 2015 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Yong Joo Kil ykil@proteinmetrics.com
*/

#include <algorithm>
#include <string>
#include <QFile>
#include <QString>
#include <QRegExp>
#include <limits>

#include <pmi_common_core_mini_debug.h>
#include "AveragineIsotopeTable.h"
#include "math_utils.h"
#include "string_utils.h"
#include "AdvancedSettings.h"
#include "math_utils_chemistry.h"
#include <Convolution1dCore.h>
//#include "ChemUtils.h"
#include "CsvWriter.h"

_PMI_BEGIN

namespace coreMini {

const static double cPI = 3.1415926535;
const static double cE_N = 2.71828183;
const static double cPeakSpacing = 1.0026; // potentially between 1.0023 - 1.0029
const static double cInversePeakSpacing = 1 / cPeakSpacing;
const static double cHugeNumber = 1e30;
const static double cTinyNumber = 1e-30;
const static double cOneOverAMillion = .000001;
const static double cInverseSqrtThousand = 1 / sqrt(1000.0);

const static int cNumberWarningMessagesOutput = 5;

static double cSinglePeakAveragineScore = 1.6; // value fixes corner case of tall single to peak to poor score
static double cErrorScore = 7.524; // random number indicates there was an error in reasigning the match score in the overlap score function

// Specific case magic numbers

static int cNumberOfPointsExpectWithinASpecificStdDev = 5;
static double cPoorCentroidingCompensateAddToStdDev = .015; // this assumed to be related to sampling, currently locked to constant sampling

const AveragineIsotopeTable g_averagIsoDist; // initializes the averagineIsotopeDistribution (doing this correctly?)

struct amino_atomic_dist IsotopicDistributions[] =
{
    {"A", {3, 5, 1, 1, 0}},
    {"R", {6, 12, 4, 1, 0}},
    {"N", {4, 6, 2, 2, 0}},
    {"D", {4, 5, 1, 3, 0}},
    {"B", {4, 6, 2, 2, 0}},
    {"C", {3, 5, 1, 1, 1}},
    {"E", {5, 7, 1, 3, 0}},
    {"Q", {5, 8, 2, 2, 0}},
    {"Z", {5, 8, 2, 2, 0}},
    {"G", {2, 3, 1, 1, 0}},
    {"H", {6, 7, 3, 1, 0}},
    {"I", {6, 11, 1, 1, 0}},
    {"L", {6, 11, 1, 1, 0}},
    {"K", {6, 12, 2, 1, 0}},
    {"M", {5, 9, 1, 1, 1}},
    {"F", {9, 9, 1, 1, 0}},
    {"P", {5, 7, 1, 1, 0}},
    {"S", {3, 5, 1, 2, 0}},
    {"T", {4, 7, 1, 2, 0}},
    {"U", {3, 5, 1, 1, 0}},
    {"W", {11, 10, 2, 1, 0}},
    {"Y", {9, 9, 1, 2, 0}},
    {"X", {6, 10, 1, 1, 0}},
    {"V", {5, 9, 1, 1, 0}},
    {"1", {0, 2, 0, 1, 0}}, /* H20 */
    {"2", {0, 1, 0, 0, 0}}, /* Hyrdogen */
    {"3", {1, 0, 0, 0, 0}}, /* Carbon */
    {"", {0,0,0,0,0}}
};

IsotopeWorkspace::IsotopeWorkspace(IsotopeCompositionRatios type) {
    int amino_dist_idx = 0;

    while (std::string(IsotopicDistributions[amino_dist_idx].letter) != std::string("")) {
        std::string letter = IsotopicDistributions[amino_dist_idx].letter;
        int *atomic_dist = IsotopicDistributions[amino_dist_idx].atomic_dist;
        amino_acid_atomic_compositions[letter] = atomic_dist;
        amino_dist_idx++;
    }

    // for averagine specifically
    averagComp.resize(IsotopeCompositionLength);
    averagComp[Carbon] = 0.3171;
    averagComp[Hydrogen] = 0.4982;
    averagComp[Nitrogen] = 0.0872;
    averagComp[Oxygen] = 0.0949;
    averagComp[Sulfur] = 0.0027;

    // element isotope ratios
    isotopeCompList.resize(IsotopeCompositionLength);

    if (type == IsotopeCompositionRatios_ByonicValuesThusFar_2015_01_26) {
        // My values
        isotopeCompList[Carbon].clear();
        isotopeCompList[Carbon].push_back(0.9893);
        isotopeCompList[Carbon].push_back(0.0107);
        isotopeCompList[Hydrogen].clear();
        isotopeCompList[Hydrogen].push_back(0.999885);
        isotopeCompList[Hydrogen].push_back(0.000115);
        isotopeCompList[Nitrogen].clear();
        isotopeCompList[Nitrogen].push_back(0.99636);
        isotopeCompList[Nitrogen].push_back(0.00364);
        isotopeCompList[Oxygen].clear();
        isotopeCompList[Oxygen].push_back(0.99757);
        isotopeCompList[Oxygen].push_back(0.00038);
        isotopeCompList[Oxygen].push_back(0.00205);
        isotopeCompList[Sulfur].clear();
        isotopeCompList[Sulfur].push_back(0.9499);
        isotopeCompList[Sulfur].push_back(0.0075);
        isotopeCompList[Sulfur].push_back(0.0425);
        isotopeCompList[Sulfur].push_back(0.0);
        isotopeCompList[Sulfur].push_back(0.0001);
    } else if (type == IsotopeCompositionRatios_ProteinProspector) {
        // Protein Prospector
        // When observing Protein Prospector, we observed that we came close to the isotopic distributions
        // of their website when we assumed the chemical formula of a amino acid includes both those atomcs in the formula
        // of the amino acids as well as the addition of both H20 and Hydrogen (within 1%)
        isotopeCompList.resize(IsotopeCompositionLength);
        isotopeCompList[Carbon].clear();
        isotopeCompList[Carbon].push_back(0.9889);
        isotopeCompList[Carbon].push_back(0.0111);
        isotopeCompList[Hydrogen].clear();
        isotopeCompList[Hydrogen].push_back(0.9998);
        isotopeCompList[Hydrogen].push_back(0.0002);
        isotopeCompList[Nitrogen].clear();
        isotopeCompList[Nitrogen].push_back(0.9963);
        isotopeCompList[Nitrogen].push_back(0.0037);
        isotopeCompList[Oxygen].clear();
        isotopeCompList[Oxygen].push_back(0.9976);
        isotopeCompList[Oxygen].push_back(0.0004);
        isotopeCompList[Oxygen].push_back(0.002);
        isotopeCompList[Sulfur].clear();
        isotopeCompList[Sulfur].push_back(0.9502);
        isotopeCompList[Sulfur].push_back(0.0075);
        isotopeCompList[Sulfur].push_back(0.0421);
        isotopeCompList[Sulfur].push_back(0.0002); //is this a javascript bug?
        isotopeCompList[Sulfur].push_back(0.0);
    } else {
        // Xcalibur
        // When observing Xcalibur, we observed that we came close to the isotopic distributions
        // of software when we assumed the chemical formula of a amino acid includes both those atomcs in the formula
        // of the amino acids as well as the addition of H20 (no additional hydrogen) (within .001%)
        isotopeCompList[Carbon].clear();
        isotopeCompList[Carbon].push_back(0.9893);
        isotopeCompList[Carbon].push_back(0.0107);
        isotopeCompList[Hydrogen].clear();
        isotopeCompList[Hydrogen].push_back(0.999885);
        isotopeCompList[Hydrogen].push_back(0.000115);
        isotopeCompList[Nitrogen].clear();
        isotopeCompList[Nitrogen].push_back(0.99632);
        isotopeCompList[Nitrogen].push_back(0.00368);
        isotopeCompList[Oxygen].clear();
        isotopeCompList[Oxygen].push_back(0.99757);
        isotopeCompList[Oxygen].push_back(0.00038);
        isotopeCompList[Oxygen].push_back(0.00205);
        isotopeCompList[Sulfur].clear();
        isotopeCompList[Sulfur].push_back(0.9499);
        isotopeCompList[Sulfur].push_back(0.0076);
        isotopeCompList[Sulfur].push_back(0.0429);
        isotopeCompList[Sulfur].push_back(0.0);
        isotopeCompList[Sulfur].push_back(0.0002);
    }

    massScaleFact = 7.1371;
}

/*!
 * \brief removes low intensity peaks for memory saving and speed purposes
 * \param rVal threshold for removal from end of array
 */
static bool memoryTrim(std::vector<double> & isoVector, double rVal)
{
    if (isoVector.empty()) {
        warningCoreMini() << "isoVector is empty";
        return false;
    }
    while(isoVector.back() < rVal) {
        isoVector.pop_back();
        if (isoVector.empty()) {
            warningCoreMini() << "isoVector is empty";
            return false;
        }
    }
    return true;
}

/*!
 * \brief convolves the current spectrum with a particular element
 * \param numberOfElement number of a particular element in composition
 * \param elemVector the isotope distribution of the element
 * \param averagPeaks the intermediate averagine distribution
 * \return true if worked, false if improper input
 */
static bool addElement(double interIsoTrim, int numberOfElement, const std::vector<double> & elemVector, std::vector<double> & averagIsoPeaks)
{
    if (elemVector.empty() || averagIsoPeaks.empty() || numberOfElement < 1) {
      return false;
    }
    for (int i = 0; i < numberOfElement; i++) {
       std::vector<double> out;
       core::convForReal(averagIsoPeaks, elemVector, out);
       std::swap(averagIsoPeaks, out);
       memoryTrim(averagIsoPeaks, interIsoTrim);
    }
    if (averagIsoPeaks.empty())
        return false;
    return true;
}

/*!
 * \brief constructs isotopes list for distribution with elements in addition to isotopesList
 * \param isoWork the workspace for elements
 * \param numberOfNewElements elements to add to the existing isotopesList
 * \param isotopesList new isotope relative intensity distribution
 */
void constructNextIsotope(double interIsoTrim, const IsotopeWorkspace & isoWork, const std::vector<int> & numberOfNewElements,
                          std::vector<double> & isotopesList, std::vector<int> &atomic_formula) {

    for (int i = IsotopeWorkspace::FirstElement; i < IsotopeWorkspace::IsotopeCompositionLength; i++) {
        int number_of_element = numberOfNewElements[i];
        const std::vector<double> & element_isotopic_ratios = isoWork.isotopeCompList[i];
        addElement(interIsoTrim, number_of_element, element_isotopic_ratios, isotopesList);
        atomic_formula[i] += numberOfNewElements[i];
    }
}

const std::vector<double> & SpecificIsotopeDistribution::getIsoList() const
{
    return m_isotopeDist;
}

void SpecificIsotopeDistribution::PrintOutIsoList() {
    for (unsigned int i = 0; i < m_isotopeDist.size(); ++i) {
        std::cout << m_isotopeDist[i];
    }
}

SpecificIsotopeDistribution::SpecificIsotopeDistribution(const std::string &amino_acid_sequence, bool add_water, int charge, double interIsoTrim) {

    IsotopeWorkspace isoWork(IsotopeWorkspace::IsotopeCompositionRatios_Xcalibur);
    std::vector<int> atomic_formula(IsotopeWorkspace::IsotopeCompositionLength, 0);
    std::vector<double> isotopesList(1, 1.0);

    int len = IsotopeWorkspace::IsotopeCompositionLength;

    //for (std::string::iterator aas_it = amino_acid_sequence.begin(); aas_it != amino_acid_sequence.end(); ++aas_it) { ///CAUSE no viable conversion error
    for(size_t slen = 0; slen < amino_acid_sequence.size(); ++slen) {
        char letter = amino_acid_sequence[slen]; //*aas_it;
        std::string letter_string(1, letter);
        if (isoWork.amino_acid_atomic_compositions.find(letter_string) != isoWork.amino_acid_atomic_compositions.end()) {
            int *isoItems = isoWork.amino_acid_atomic_compositions[letter_string];

            std::vector<int> numberOfElements(isoItems, isoItems + len); // hack array to vector
            constructNextIsotope(interIsoTrim, isoWork, numberOfElements, isotopesList, atomic_formula);
        }
    }
    if (add_water) {
        int *water_atoms = isoWork.amino_acid_atomic_compositions["1"];
        std::vector<int> water_atoms_vector(water_atoms, water_atoms + len);
        constructNextIsotope(interIsoTrim, isoWork, water_atoms_vector, isotopesList, atomic_formula);
    }

    for (int i = 0; i < charge; ++i) {
        int *hydrogen_atoms = isoWork.amino_acid_atomic_compositions["2"];
        std::vector<int> hydrogen_atoms_vector(hydrogen_atoms, hydrogen_atoms + len);
        constructNextIsotope(interIsoTrim, isoWork, hydrogen_atoms_vector, isotopesList, atomic_formula);
    }
    m_isotopeDist = isotopesList;
}

/*!
 * \brief creates individual isotope list
 * \param mass mass of averagine distribution
 * \return averagine distribution
 */
const std::vector<double> & AveragineIsotopeTable::isotopeIntensityFractions(double mass) const
{
    static std::vector<double> zeroTable;  //Note: not thread safe!

    int index = isotopeIntensityFractionsIndexRound(mass);
    if (index < 0) {
        debugCoreMini() << "m_averagTable size is <= 0";
        return zeroTable;
    }
    return m_averagTable[index];
}

int AveragineIsotopeTable::isotopeIntensityFractionsIndexRound(double mass) const
{
    if (m_averagTable.size() <= 0) {
        return -1;
    }
    int index = pmi::round(mass / m_parameters.massSpacing);
    if (index < 0) {
        index = 0;
    }
    if (index >= (int)m_averagTable.size()) {
        index = (int)m_averagTable.size() - 1;
    }
    return index;
}

//alpha of 0 means exactly on floor. alpha of 1 means exactly on floor + 1.
int AveragineIsotopeTable::isotopeIntensityFractionsIndexFloor(double mass, double * alphaBlend) const
{
    if (m_averagTable.size() <= 0) {
        return -1;
    }
    const int size = static_cast<int>(m_averagTable.size());

    //mass is 320.1 and m_massSpacing is 100
    //indexF is 3.201
    double indexF = mass / m_parameters.massSpacing;
    //index is 3
    int index = std::floor(indexF);

    if (index < 0) {
        //out of bounds on left
        index = 0;
        *alphaBlend = 0;
    } else if (index >= size) {
        //out of bounds on right
        index = size - 1;
        *alphaBlend = 0; //On edge, can't blend. But keep it so it defines itself as best value
    } else if (index == size -1){
        //last index. nothing to blend with
        *alphaBlend = 0; //On edge, can't blend. But keep it so it defines itself as best value
    } else {
        *alphaBlend = indexF - index;
    }
    return index;
}

double ChargeScoreInfo::getPointsIntensBetweenIndices(TestParameters &test_parameters, int window_start, int window_end) const {
    double intensity = 0;
    for (int mpv_index = 0; mpv_index < (int)matchPointsVector.size(); mpv_index++) {
        const point2d & matchPoint = matchPointsVector[mpv_index];
        if (matchVector[mpv_index].index >= window_start && matchVector[mpv_index].index <= window_end) {
            double percent_accounted_for = matchVector[mpv_index].percent_account_for < 1 ? matchVector[mpv_index].percent_account_for : 1;
            if (test_parameters.current_match_list.checkPointExcludedUnsorted(matchVector[mpv_index].index - window_start)) {
                intensity += .3*matchPoint.y()*percent_accounted_for;  // .3 is the penalty for being an already accounted for peak
                                                                       // really aught to look at the other one an see how well accounted for
            }
            else {
                intensity += matchPoint.y()*percent_accounted_for;
            }
        }
    }
    return intensity;
}

double ChargeScoreInfo::getPointsIntensBetweenIndices(int window_start, int window_end) const {
    double intensity = 0;
    for (int mpv_index = 0; mpv_index < (int)matchPointsVector.size(); mpv_index++) {
        point2d matchPoint = matchPointsVector[mpv_index];
        if (matchVector[mpv_index].index >= window_start && matchVector[mpv_index].index <= window_end) {
            double percent_accounted_for = matchVector[mpv_index].percent_account_for < 1 ? matchVector[mpv_index].percent_account_for : 1;
            intensity += matchPoint.y()*percent_accounted_for;
        }
    }
    return intensity;
}

/*!
 * \brief creates an averagine isotope distribution at each mass
 * \param isoWork
 */
void AveragineIsotopeTable::construstAveragineIsotopes(IsotopeWorkspace & isoWork)
{
    m_averagTable.reserve(m_parameters.numberAveragDists);

    std::vector<int> atomic_formula(IsotopeWorkspace::IsotopeCompositionLength, 0);

    std::vector<double> averagIsotopesList_prev;
    std::vector<double> averagIsotopesList_curr(1, 1.0); // initialize the case of no elements ([1], or all mono)
    std::vector<int> numberOfElements_prev(IsotopeWorkspace::IsotopeCompositionLength, 0);
    std::vector<int> numberOfElements_curr(IsotopeWorkspace::IsotopeCompositionLength, 0);
    for (int j = 0; j < m_parameters.numberAveragDists; j++) {
        std::vector<int> numberOfElementsDiff(IsotopeWorkspace::IsotopeCompositionLength, 0);
        // calculate number more of each isotope
        for (int i = IsotopeWorkspace::FirstElement; i < IsotopeWorkspace::IsotopeCompositionLength; i++) {
            double averagMass = j*m_parameters.massSpacing;
            numberOfElements_curr[i] = round(averagMass * isoWork.averagComp[i] / isoWork.massScaleFact);
            numberOfElementsDiff[i] = numberOfElements_curr[i] - numberOfElements_prev[i];
        }
        // add onto the previous isotope distribution
        constructNextIsotope(m_parameters.interIsoTrim, isoWork, numberOfElementsDiff, averagIsotopesList_curr, atomic_formula);
        std::vector<double> averagIsotopesList_trimmed = averagIsotopesList_curr;
        memoryTrim(averagIsotopesList_trimmed, m_parameters.finalIsoTrim);
        m_averagTable.push_back(averagIsotopesList_trimmed);
        averagIsotopesList_prev = averagIsotopesList_curr;
        numberOfElements_prev = numberOfElements_curr;
    }
}

AveragineIsotopeTable::AveragineIsotopeTable() {
  m_parameters = Parameters::makeDefaultForByomapFeatureFinder();

  // initialize the table of averagine distributions
  IsotopeWorkspace isoWork(
      IsotopeWorkspace::
          IsotopeCompositionRatios_ByonicValuesThusFar_2015_01_26);
  construstAveragineIsotopes(isoWork);
}

AveragineIsotopeTable::AveragineIsotopeTable(
    const Parameters &parameter) {
  m_parameters = parameter;

  // initialize the table of averagine distributions
  IsotopeWorkspace isoWork(
      IsotopeWorkspace::
          IsotopeCompositionRatios_ByonicValuesThusFar_2015_01_26);
  construstAveragineIsotopes(isoWork);
}

// TODO: migrate this to math_utils.h. Note that it's made inline to avoid
// potential link issue.
/*!
 * \brief lerp linear interpolation between a and b with parameter alpha
 * \param a any real value
 * \param b any real value
 * \param alpha between 0 and 1, inclusive. alpha of 0 will return value a and
 * alpha of 1 returns value b. \return
 */
inline double lerp(double a, double b, double alpha)
{
    return (1 - alpha) * a + alpha * b;
}

// Note(Yong,2018-05-18):
// Because we are creating caches at specific mass values, we need to
// interpolate to get more accurate isotope ratio values. The interpolation, I
// had to think if this will continue to hold probability distribution by having
// it the sums equal to one. The proof below shows that linear interpolated
// values continue to show this property. Vector X is composed of
// x1,x2,x3,...,xk. Assume Sum(X) = 1 Vector Y is composed of y1,y2,y3,...,yk.
// Assume Sum(Y) = 1
// alpha is between 0 and 1.
// Interpolated vector: Z = (1-alpha) * X + alpha * Y.
// Claim: Sum(Z) = 1.
//
//  Assume: Sum(X) = (x1 + x2 + x3 ... + xk) = 1, same for Sum(Y) = 1.
//  Observe:
//  (1-alpha) * Sum(X) = (1-alpha) * 1.
//  (alpha) * Sum(Y) = (alpha) * 1.
//
//  Sum(Z) = z1 + z2 + z3 ... + zk
//   = ((1-alpha) * x1 + (alpha) * y1) + ... + ((1-alpha) * xk + (alpha) * yk)
//         = (1-alpha) * (x1 + ... + xk) + (alpha) * (y1 + ... + yk)
//         = (1-alpha) * Sum(X) + (alpha) * Sum(Y) =  (1-alpha) * 1 + (alpha) *
//         1 = 1
double AveragineIsotopeTable::isotopeFractionInterpolated(double mass, int isotope) const
{
    if (isotope < 0 || mass < 0) {
        return 0;
    }
    const int size = static_cast<int>(m_averagTable.size());
    double alphaBlend = 0;
    double finalValue = 0;
    int index = isotopeIntensityFractionsIndexFloor(mass, &alphaBlend);
    if (index >= 0 && index < size - 1) {
        double isotopeValueLeft = 0;
        double isotopeValueRight = 0;
        const std::vector<double> &isotopesLeft = m_averagTable[index];
        const std::vector<double> &isotopesRight = m_averagTable[index + 1];

        if (isotope < static_cast<int>(isotopesLeft.size())) {
            isotopeValueLeft = isotopesLeft[isotope];
        }
        if (isotope < static_cast<int>(isotopesRight.size())) {
            isotopeValueRight = isotopesRight[isotope];
        }
        finalValue = lerp(isotopeValueLeft, isotopeValueRight, alphaBlend);
    }
    return finalValue;
}

void AveragineIsotopeTable::saveToFile(const QString &filename) const
{
    CsvWriter writer(filename);

    const bool valid = writer.open();
    if (valid) {
        QStringList row;
        const std::size_t size = m_averagTable.size();
        row << "MassDalton";
        if (size <= 0) {
            return;
        }
        for (std::size_t i = 0; i < m_averagTable.back().size(); i++) {
            row << QStringLiteral("iso%1").arg(i);
        }
        writer.writeRow(row);
        for (std::size_t i = 0; i < size; i++) {
            row.clear();
            row << QString::number(i * m_parameters.massSpacing);
            for (const double val : m_averagTable[i]) {
                row << QString::number(val, 'g', 12);
            }
            writer.writeRow(row);
        }
    }

    return;
}

std::vector<std::vector<double>> AveragineIsotopeTable::averagineTable()const {
    return m_averagTable;
}

/*!
 * \brief calculates the intensity of the averagine distribution at a particular mz
 * \param mz mz position
 * \param clamping the cutoff in number of stdDevs where we treat the averagine dist
 * \return return intensity of averagine
 */
double AveragineIsotopeFunction::evaluate(double mz, int clamping) const
{
    if (clamping > 0) {
        double e_Position = (mz - m_mzOffset)*m_charge*cInversePeakSpacing;
        double e_stdDev = (m_stdDev+cPoorCentroidingCompensateAddToStdDev)
                          *cInversePeakSpacing*m_charge;
        double e_leftBound = e_Position - clamping*e_stdDev;
        double e_rightBound = e_Position + clamping*e_stdDev;
        int e_startIdx = pmi::Ceil(e_leftBound);
        int e_endIdx = pmi::Floor(e_rightBound);
        if (e_startIdx < 0) {
            e_startIdx = 0;
        }
        if (e_endIdx > (int)m_averagIsoDist.size() - 1) {
            e_endIdx = (int)m_averagIsoDist.size() - 1;
        }
        if (e_startIdx > e_endIdx) {
            return 0;
        }

        double averagFuncVal = 0;
        for (int peakNumber = e_startIdx; peakNumber <= e_endIdx; peakNumber++) {
            double a = e_Position - peakNumber;
            double b = -a*a;
            double c = 2*e_stdDev*e_stdDev;
            averagFuncVal += m_averagIsoDist[peakNumber]*pow(cE_N,b / c);
        }
        return m_scaleFact*averagFuncVal;
    }

    // no clamping
    double averagFuncVal = 0;
    for (int i = 0; i < (int)m_averagIsoDist.size(); i++) {
        double b = (m_mzOffset + i*cPeakSpacing / m_charge);
        double val = m_averagIsoDist[i]*pow(cE_N,-(mz - b)
                     *(mz - b) / (2*m_stdDev*m_stdDev));
        averagFuncVal += val;
    }
    return m_scaleFact*averagFuncVal;
}

/*!
 * \brief Returns the corresponding averagine peak for a spectrum peak
 * \param mz position
 * \return index of peak
 */
int AveragineIsotopeFunction::peakOverlapIndex(double mz) const
{
    double mz_in_peak_space = (mz - m_mzOffset)*m_charge*cInversePeakSpacing;
    double peak_space_stdDev = (m_stdDev+cPoorCentroidingCompensateAddToStdDev)*m_charge*cInversePeakSpacing;

    //TODO(Yong): I don't understand what these Floor() calls are suppose to do.
    //Please provide some detail and maybe a function to to provide meaning to the below line.
    if (Floor(mz_in_peak_space + peak_space_stdDev) > Floor(mz_in_peak_space - peak_space_stdDev)) {
        int potential_index = pmi::round(mz_in_peak_space);
        if (potential_index < (int)averagIsoDist().size()) {
            return potential_index;
        }
    }
    return -1;
}

AveragineIsotopeTable::Parameters
AveragineIsotopeTable::Parameters::makeDefaultForAccurate() {
  AveragineIsotopeTable::Parameters obj;
  obj.interIsoTrim = 1e-8;
  obj.finalIsoTrim = 1e-7;
  obj.massSpacing = 10;
  obj.numberAveragDists = 1000;
  return obj;
}

AveragineIsotopeTable::Parameters
AveragineIsotopeTable::Parameters::makeDefaultForByomapFeatureFinder() {
  AveragineIsotopeTable::Parameters obj;
  obj.interIsoTrim = 0.0001;
  obj.finalIsoTrim = 0.001;
  obj.massSpacing = 100;
  obj.numberAveragDists = 1000;
  return obj;
}

QString AveragineIsotopeTable::Parameters::toStringDevFriendly() const {
  QString str = QStringLiteral("massspacing_%1__avgDist_%2__interTrim_%3__finalTrim_%4").arg(massSpacing).arg(numberAveragDists).arg(interIsoTrim).arg(finalIsoTrim);
  return str;
}

/*!
 * \brief constructs an individual averagine at mz position
 * \param isoList list of averagine peaks for near mass
 */
AveragineIsotopeFunction::AveragineIsotopeFunction(const std::vector<double> & isoDist) :
    m_averagIsoDist(isoDist)
{
    m_charge = -1;
    m_stdDev = -1;
    m_mzOffset = -std::numeric_limits<double>::max();
    m_scaleFact = -1;
}


struct PeakTagSort {
    point2d point;
    int specPointIndex;
    int rankPointIndex;
};

inline bool lessScore(const ChargeScoreInfo & p1, const ChargeScoreInfo & p2) {
    return p1.getScore() < p2.getScore();
}

inline bool lessY(const PeakTagSort & pt1, const PeakTagSort & pt2) {
    return pt1.point.y() < pt2.point.y();
}

inline bool lessIndex(const PeakTagSort & pt1, const PeakTagSort & pt2) {
    return pt1.specPointIndex < pt2.specPointIndex;
}

void PeakTagList::addPoints2dList(const point2dList & specPointsList) // this is really slow...
{
    clear();

    int specPointsListSize = static_cast<int>(specPointsList.size());
    std::vector<PeakTagSort> specPointsListSorted;

    m_windowPoints.reserve(specPointsListSize);
    specPointsListSorted.reserve(specPointsListSize);

    for (int spl_index = 0; spl_index < specPointsListSize; spl_index++) {
        PeakTagSort peak_tag_sort_and_index;
        peak_tag_sort_and_index.point = specPointsList[spl_index];
        peak_tag_sort_and_index.specPointIndex = spl_index;
        specPointsListSorted.push_back(peak_tag_sort_and_index);
        specPointsListSorted[spl_index].specPointIndex = spl_index;
    }
    qSort(specPointsListSorted.rbegin(), specPointsListSorted.rend(), lessY);

    for (int i = 0; i < specPointsListSize; i++) {
        specPointsListSorted[i].rankPointIndex = i;
        const point2d & specPoint = specPointsListSorted[i].point;
        double xPoint = specPoint.x();
        double yPoint = specPoint.y();
        bool excludeBool = false;
        PeakTag windowPoint(xPoint, yPoint, excludeBool, i);
        m_windowPoints.push_back(windowPoint);
    }
    qSort(specPointsListSorted.begin(), specPointsListSorted.end(), lessIndex);

    for (int spls_index = 0; spls_index < specPointsListSize; spls_index++) {
        m_windowPointsIndices.push_back(specPointsListSorted[spls_index].rankPointIndex);
    }
}

point2d PeakTagList::getWindowPointSorted(int pointToGetIndx)
{
    point2d window2dPoint (m_windowPoints[pointToGetIndx].xVal, m_windowPoints[pointToGetIndx].yVal);
    return window2dPoint;
}

point2d PeakTagList::getWindowPoint(int pointToGetIndx)
{
    int windowPointIndex = m_windowPointsIndices[pointToGetIndx];
    point2d window2dPoint (m_windowPoints[windowPointIndex].xVal, m_windowPoints[windowPointIndex].yVal);
    return window2dPoint;
}

void PeakTagList::excludePoint(int excludedPointIndx)
{
    int windowPointIndex = m_windowPointsIndices[excludedPointIndx];
    m_windowPoints[windowPointIndex].excluded = true;
}

// there are "sorted" and "unsorted" versions in order to exclude points from scopes where
// the points in the tag list are sorted by y (sorted) and sorted by x (unsorted), where
// "sorted" refers to the way sorted in the class
bool PeakTagList::checkPointExcludedUnsorted(int pointToCheckIndx)
{
    if (pointToCheckIndx < 0 || pointToCheckIndx >= m_windowPoints.size()) {
        static int count = 0;
        if (count == cNumberWarningMessagesOutput - 1) {
            warningCoreMini() << "last index out of bounds warning ";
        }
        if (count < cNumberWarningMessagesOutput) {
            warningCoreMini() << "index out of bounds with" <<  pointToCheckIndx;
        }
        count++;
        return false;
    }
    int windowPointIndex = m_windowPointsIndices[pointToCheckIndx];
    return m_windowPoints[windowPointIndex].excluded;
}

bool PeakTagList::checkPointExcludedSorted(int pointToCheckIndx)
{
    if (pointToCheckIndx < 0 || pointToCheckIndx >= m_windowPoints.size()) {
        static int count = 0;
        if (count == cNumberWarningMessagesOutput - 1) {
            warningCoreMini() << "last index out of bounds warning";
        }
        if (count < cNumberWarningMessagesOutput) {
            warningCoreMini() << "index out of bounds with" <<  pointToCheckIndx;
        }
        return false;
    }
    return m_windowPoints[pointToCheckIndx].excluded;
}

void PeakTagList::setWindowPeakOffset(int window_peak_offset) {
    m_window_peak_offset_from_specpoints = window_peak_offset;
}

int PeakTagList::getWindowPeakOffsetFromSpecpoints() {
    return m_window_peak_offset_from_specpoints;
}

int PeakTagList::getSize() const
{
    int windowPointsSize = m_windowPoints.size();
    return windowPointsSize;
}

bool PeakTagList::checkAllPointsExcluded()
{
    for (int pointIndex = 0; pointIndex < m_windowPointsIndices.size(); pointIndex++) {
        if (checkPointExcludedSorted(pointIndex) == false) {
            return false;
        } else {
            continue;
        }
    }
    return true;
}

PeakTagList::PeakTagList()
{
    m_window_peak_offset_from_specpoints = 0;
}

/*!
 * \brief finds the index of smallest element in array
 * \param isoDist isotope distribution
 * \return index of smallest element
 */
template<typename T>
int findMinIndex(const std::vector<T> & list)
{
    if (list.size() <= 0)
        return -1;
    int minIndex = 0;
    for (int i = 1; i < (int)list.size(); i++) {
        if (list[i] < list[minIndex])
            minIndex = i;
    }
    return minIndex;
}

/*!
 * \brief findMaxIndex finds the index of largest element in array
 * \param isoDist isotope distribution
 * \return index of largest element
 */
PMI_COMMON_CORE_MINI_EXPORT  int findMaxIndex(const std::vector<double> & list)
{
    if (list.size() <= 0) {
        debugCoreMini() << "******************************************** findMaxIndex returning -1";
        return -1;
    }
    int maxIndex = 0;
    for (int i = 1; i < (int)list.size(); i++) {
        if (list[i] > list[maxIndex])
            maxIndex = i;
    }
    return maxIndex;
}

/*!
 * \brief determines the standard deviation at an offset
 * \param
 */
PMI_COMMON_CORE_MINI_EXPORT  double findStdDev(double uncertainty, double mzOffset, UncertaintyScaling uncertainty_scaling_type)
{
    double found_stdDev;
    if (mzOffset <= 0) {
        debugCoreMini() << "assigning stdDev before assigning mzOffset, returning invalid value";
        found_stdDev = 0; // this should give no matches, all bad scores
        return found_stdDev;
    }
    if (uncertainty_scaling_type == ConstantUncertainty) {
        found_stdDev = uncertainty; // expect Daltons
    } else if (uncertainty_scaling_type == SquareRootUncertainty) {
        found_stdDev = uncertainty*sqrt(mzOffset);
    } else { // if == 2 == LinearUncertainty, change if add another
        found_stdDev = uncertainty*cOneOverAMillion*mzOffset;
    }
    return found_stdDev;
}

const static double cSamplingType1OuterBound = .8;
const static double cSamplingType2OuterBound = 1.2;
const static double cSamplingType3OuterBound = 3;

const static double cSamplingType1SamplingRatio = 1;
const static double cSamplingType2SamplingRatio = 2;
const static double cSamplingType3SamplingRatio = 4;

/*!
 * \brief evaluates isoFunc at regular points in the domain
 * \param isoFunc
 * \param start_mz starting position of trace
 * \param end_mz ending position of trace
 * \param stepSize interval between evaluated points
 * \param averagineSampling array of points tangent to averagine function
 */
PMI_COMMON_CORE_MINI_EXPORT  void sampleAveragine(AveragineIsotopeFunction & isoFunc, double start_mz,
                    double end_mz, double stepSize, point2dList & averagineSampling)
{
    averagineSampling.clear();
    if (start_mz > end_mz || start_mz < 0 || stepSize <= 0) {
        debugCoreMini() << "Samping start or end indicies or step size is incorrect";
    }
    double stepSize_adaptive = stepSize;
    for (double mz = start_mz; mz < end_mz + stepSize_adaptive; mz += stepSize_adaptive) {
        double integerSpaceMZ = (mz - isoFunc.mzOffset()) * isoFunc.charge();
        double floorNearestMono = Floor((mz - isoFunc.mzOffset()) * isoFunc.charge());
        double ceilNearestMono = Ceil((mz - isoFunc.mzOffset()) * isoFunc.charge());

        if (Floor(integerSpaceMZ - cSamplingType3OuterBound*isoFunc.stdDev()) >= floorNearestMono &&
            Ceil(integerSpaceMZ + cSamplingType3OuterBound*isoFunc.stdDev()) <= ceilNearestMono) {
            stepSize_adaptive = stepSize*cSamplingType3SamplingRatio;
        }
        else if (Floor(integerSpaceMZ - cSamplingType2OuterBound*isoFunc.stdDev()) == floorNearestMono &&
                 Floor(integerSpaceMZ - cSamplingType3OuterBound*isoFunc.stdDev()) < floorNearestMono
                 || Ceil(integerSpaceMZ + cSamplingType2OuterBound*isoFunc.stdDev()) == ceilNearestMono &&
                 Ceil(integerSpaceMZ + cSamplingType3OuterBound*isoFunc.stdDev()) < ceilNearestMono) {
                 stepSize_adaptive = stepSize*cSamplingType2SamplingRatio;
        }
        else if (Floor(integerSpaceMZ - cSamplingType1OuterBound*isoFunc.stdDev()) == floorNearestMono &&
                 Floor(integerSpaceMZ - cSamplingType2OuterBound*isoFunc.stdDev()) < floorNearestMono
                 || Ceil(integerSpaceMZ + cSamplingType1OuterBound*isoFunc.stdDev()) == ceilNearestMono &&
                 Ceil(integerSpaceMZ + cSamplingType2OuterBound*isoFunc.stdDev()) < ceilNearestMono) {
                 stepSize_adaptive = stepSize*cSamplingType2SamplingRatio;
        }
        else if (Floor(integerSpaceMZ - cSamplingType1OuterBound*isoFunc.stdDev()) < floorNearestMono ||
                 Ceil(integerSpaceMZ + cSamplingType1OuterBound*isoFunc.stdDev()) > ceilNearestMono) {
                 stepSize_adaptive = stepSize;
        }
        point2d insertValue = point2d(mz, isoFunc.evaluate(mz, cSamplingType3SamplingRatio));
        averagineSampling.push_back(insertValue);
    }
}

const static double cMaxPercentAccountFor = 3.0;
const static double cMinPercentAccountFor = 0.2;

/*!
 * \brief takes list of spectrum points found to match to the same averagine peak and assigns them to appropriate categories
 * \param specPoints is spectrum points
 * \param isoFunc is averagine function
 * \param inAveragPeakNum averagine peak inte_matchVector points matched to
 * \param inter_matchVector is list of indicies of matching spectrum peaks
 * \param inVector points matched to averagine peak insufficently well to be considered match
 * \param matchVector points matched to averagine peak sufficiently good match
 * \param averagHitVector averagine peaks with a spectrum point matched
 */
PMI_COMMON_CORE_MINI_EXPORT  void findBestOverlap(const point2dList & specPoints, const AveragineIsotopeFunction & isoFunc,
                int inAveragPeakNum, const std::vector<int> & inter_matchVector,
                std::vector<int> & inVector, std::vector<PointAndPercentAccountFor> & matchVector,
                std::vector<int> & averagHitVector)
{
    if (inAveragPeakNum < 0) {
        return;
    }
    // find the index that most closely matches the theoretical
    const double averageIntes = isoFunc.scaleFact()*isoFunc.averagIsoDist()[inAveragPeakNum];
    double closestSpecPoint_y = -std::numeric_limits<float>::max(); // 3.40282e+38
    int closestSpecPointIndex = -1;
    for (int inter_matchIndex = 0; inter_matchIndex < (int)inter_matchVector.size(); inter_matchIndex++) {
        const int inter_matchSpecIndex = inter_matchVector[inter_matchIndex];
        const point2d specPoint = specPoints[inter_matchSpecIndex];
        const double specPoint_y = specPoint.y();
        if (std::abs(averageIntes - specPoint_y) < std::abs(averageIntes - closestSpecPoint_y)) {
            closestSpecPoint_y = specPoint_y;
            closestSpecPointIndex = inter_matchSpecIndex;
        }
    }
    // put the points in the appropriate lists
    for (int inter_matchIndex = 0; inter_matchIndex < (int)inter_matchVector.size(); inter_matchIndex++) {
        const int inter_matchSpecIndex = inter_matchVector[inter_matchIndex];
        if (inter_matchSpecIndex == closestSpecPointIndex) {
            const point2d & specPoint = specPoints[inter_matchSpecIndex];
            const double specPoint_x = specPoint.x();

            PointAndPercentAccountFor match;
            match.index = inter_matchSpecIndex;
            //match.percent_account_for = isoFunc.evaluate(specPoint_x, 1) / specPoint.y(); // alternative method
            match.percent_account_for = isoFunc.scaleFact()*isoFunc.averagIsoDist()[inAveragPeakNum] / specPoint.y();
            if (match.percent_account_for < cMaxPercentAccountFor) {
                matchVector.push_back(match);
                if (specPoint.y() > cMinPercentAccountFor*isoFunc.evaluate(specPoint_x, 1)) {
                    averagHitVector.push_back(inAveragPeakNum);

                    // double check list is sorted
                    if (averagHitVector.size() > 1) {
                        if (averagHitVector[averagHitVector.size()-1] < averagHitVector[averagHitVector.size()-2]) {
                            std::sort(averagHitVector.begin(), averagHitVector.end());
                        }
                    }
                }
            }
            else {
                inVector.push_back(inter_matchSpecIndex);
            }
        }
        else {
            inVector.push_back(inter_matchSpecIndex);
        }
    }
}

/*!
 * \brief Get most intense point within bounds
 * \param ms1Centroided is input spectrum
 * \param leftBound is leftmost bound of points considered
 * \param rightBound is rightmost bound of points considered
 * \return most intense point within bounds (-1,-1) if no match
 */
PMI_COMMON_CORE_MINI_EXPORT  void getMostIntensePointWithinBound(const PlotBase &ms1Centroided,
                                                           double leftBound, double rightBound,
                                                           point2d &bestPointInSpectrum, bool *ok)
{
    Q_ASSERT(ok);
    *ok = false;
    const point2dList specPointsInBounds = ms1Centroided.getDataPoints(leftBound, rightBound, true);
    int specPointsInBoundsSize = (int)specPointsInBounds.size();

    // find largest point in the interval
    int highestYIndex = -1;
    if (specPointsInBoundsSize > 0) {
        *ok = true;
        double highestYVal = -1;
        for (int j = 0; j < specPointsInBoundsSize; j++) {
            point2d specPointInBounds = specPointsInBounds[j];
            double curYVal = specPointInBounds.y();
            if (highestYVal < curYVal) {
                highestYVal = curYVal;
                highestYIndex = j;
            } else {
                continue;
            }
        }
        bestPointInSpectrum = specPointsInBounds[highestYIndex];
    }
}

PMI_COMMON_CORE_MINI_EXPORT  double find_penalty_scale_factor_if_accounted_for(TestParameters & test_parameters, int index,
                              const double if_already_accounted_for_scale_factor) {
    int windowPointsIndex = index - test_parameters.current_match_list.getWindowPeakOffsetFromSpecpoints();
    if (windowPointsIndex > 0 && windowPointsIndex < test_parameters.current_match_list.getSize()) {
        if (test_parameters.current_match_list.checkPointExcludedSorted(windowPointsIndex) == true) {
            return if_already_accounted_for_scale_factor;
        }
    }
    return 1;
}

/*!
 * \brief determinePointsClassifications classify points into bins to be scored differently
 * \param specPoints experimental x,y values
 * \param specStartIndx start index of window we are classifying
 * \param specEndIndx last index in window we are classifying
 * \param isoFunc theorietical isotopic distribution
 * \param inter_matchVector vector containing points that match to a peaks (assigned after
 * \      comparing those that correspond to same point
 */
void determinePointsClassifications(const point2dList & specPoints, pt2idx specStartIndx ,pt2idx specEndIndx,
                                    const AveragineIsotopeFunction & isoFunc, std::vector<int> & inter_matchVector,
                                    std::vector<int> & offVector, std::vector<int> & inVector,
                                    std::vector<PointAndPercentAccountFor> & matchVector, std::vector<int> & averagHitVector)
{
    int inAveragPeakNum = -1;
    int peakMatchNumber = INT_MAX;
    for (int specIndex = (int)specStartIndx; specIndex <= (int)specEndIndx; specIndex++) {
        const point2d & specPoint = specPoints[specIndex];
        double specPoint_x = specPoint.x();
        inAveragPeakNum = isoFunc.peakOverlapIndex(specPoint_x);

        if (inAveragPeakNum == peakMatchNumber) {
            inter_matchVector.push_back(specIndex);
        } else {
            if (inter_matchVector.size() > 0) {
                findBestOverlap(specPoints, isoFunc, peakMatchNumber, inter_matchVector, inVector,
                                matchVector, averagHitVector);
                inter_matchVector.clear();
            }
            if (inAveragPeakNum <= -1) {
                offVector.push_back(specIndex);
                continue;
            } else {
                inter_matchVector.push_back(specIndex);
                peakMatchNumber = inAveragPeakNum;
            }
        }
    }
    if (inter_matchVector.size() > 0) {
        findBestOverlap(specPoints, isoFunc, inAveragPeakNum, inter_matchVector, inVector,
                        matchVector, averagHitVector);
    }
}

PMI_COMMON_CORE_MINI_EXPORT  bool calcOverlapScoreInputCheck(const PlotBase & ms1Centroided,
                                                       const AveragineIsotopeTable & averagIsoDist,
                           ChargeScoreInfo & overlapObject) {

    double mzOffset = overlapObject.getMzOffset();
    int zCharge = overlapObject.getCharge();
    double scaleFact = overlapObject.getScaleFact();

    const point2dList & specPoints = ms1Centroided.getPointList();
    if (specPoints.size() <= 0) {
        debugCoreMini() << "specPoints length <= 0";
        return false;
    }
    const std::vector<double> & averagDist = averagIsoDist.isotopeIntensityFractions(mzOffset*zCharge);
    if (averagDist.size() <= 0) {
        debugCoreMini() << "averageDist not found";
        return false;
    }
    if (mzOffset <= std::numeric_limits<int>::min()) {
        debugCoreMini() << "mzOffset has not been modified";
        return false;
    }
    if (zCharge < 1) {
        debugCoreMini() << "charge values less than 1 not permitted";
        return false;
    }
    if (scaleFact < 0) {
        debugCoreMini() << "scale factor must be a positive value";
        return false;
    }
    return true;
}

const static double cNumberStdDevsBoundaryFudge = 2.0;

/*!
 * \brief Quantifies how well averagine theoretical models experimental range
 * \param ms1Centroided is input spectrum
 * \param averagIsoDist initializes averagine function
 * \param zCharge initializes averagine function
 * \param mzOffset initializes averagine function
 * \param scaleFact initializes averagine function
 * \param overlapObject is the output monoisotope
 */
PMI_COMMON_CORE_MINI_EXPORT  void calcOverlapScore(const PlotBase & ms1Centroided, const AveragineIsotopeTable & averagIsoDist,
                  TestParameters & test_parameters, ChargeScoreInfo & overlapObject)
{
    // set to error output values
    overlapObject.matchVector.clear();
    overlapObject.matchPointsVector.clear();
    overlapObject.score = cErrorScore;

    // validate all input values
    if (!calcOverlapScoreInputCheck(ms1Centroided, averagIsoDist, overlapObject)) {
        return;
    }

    // initialize parameters
    double mzOffset = overlapObject.getMzOffset();
    int charge = overlapObject.getCharge();
    double scaleFact = overlapObject.getScaleFact();
    const point2dList & specPoints = ms1Centroided.getPointList();
    const std::vector<double> & averagDist = averagIsoDist.isotopeIntensityFractions(mzOffset*charge);
    int maxAveragPeakIndex = findMaxIndex(averagDist);
    int averagDistSize = (int)averagDist.size();

    // set up theoretical
    AveragineIsotopeFunction isoFunc(averagDist);
    isoFunc.setMzOffset(mzOffset);
    isoFunc.setCharge(charge);
    isoFunc.setScaleFact(overlapObject.getScaleFact());
    isoFunc.setStdDev(test_parameters.uncertainty, test_parameters.uncertainty_scaling_type);

    // set range of experimental values
    double fudge = cNumberStdDevsBoundaryFudge*isoFunc.stdDev();
    // getIndexLessOrEqual returns -1 if left of first point, size - 1 if right of last point
    // logic enforces mixmatch if both start and end indexs are out of bounds
    pt2idx specStartIndx = ms1Centroided.getIndexLessOrEqual(mzOffset - fudge, false) + 1;
    pt2idx specEndIndx = ms1Centroided.getIndexLessOrEqual(mzOffset + averagDistSize*cPeakSpacing / charge + fudge, false);
    if (specStartIndx > specEndIndx || specStartIndx < 0 || specEndIndx >= (int)specPoints.size()) {
        static int count = 0;
         if (count == cNumberWarningMessagesOutput - 1) {
            warningCoreMini() << "last zScore indicie mixmatch";
        }
        if (count++ < cNumberWarningMessagesOutput) {
            warningCoreMini() << "zScore start indicie" << specStartIndx << "or end indicie"
                     << specEndIndx << "mixmatched or out of bounds. Likely no values in window.";
        }
        return;
    }

    // classify all points
    std::vector<int> inter_matchVector; // temporary holder for multiple points falling within averagine peak bounds
    std::vector<int> offVector; // points out of averagine peak bounds
    std::vector<int> inVector; // points that overlap, but are not best
    std::vector<PointAndPercentAccountFor> matchVector; // points that overlap and are the best
    std::vector<int> averagHitVector; // averagine index hits

    inter_matchVector.reserve(cNumberOfPointsExpectWithinASpecificStdDev);
    offVector.reserve(specEndIndx - specStartIndx + 1);
    inVector.reserve(cNumberOfPointsExpectWithinASpecificStdDev);
    matchVector.reserve(averagDistSize);
    averagHitVector.reserve(averagDistSize);

    determinePointsClassifications(specPoints, specStartIndx, specEndIndx, isoFunc, inter_matchVector,
                                   offVector, inVector, matchVector, averagHitVector);

    // prevent matches for low mass peptides that only match to a single peak
    if (averagHitVector.size() <= 1) {
        overlapObject.score = cSinglePeakAveragineScore + .0001*charge; // reduces qSort operations and allows debugging
        overlapObject.matchVector = matchVector;
        return;
    }

    // Seed values for score calculation
//    double offVectorScoreDenominator_charge = 2;
    double offVectorScoreDenominator_position = 2;
//    double offVectorScorePenaltyRatio_charge = 1.60 / .75;
    double offVectorScorePenaltyRatio_position = 1.60 / .75;

//    double inVectorScoreDenominator_charge = .5;
    double inVectorScoreDenominator_position = .5;
//    double inVectorScorePenaltyRatio_charge = .85 / .5;
    double inVectorScorePenaltyRatio_position = .85 / .5;

//    double matchVectorScoreDenominator_charge = 1.2;
    double matchVectorScoreDenominator_position = 1.2;
//    double matchVectorScorePenaltyRatio_charge = 2.25;
    double matchVectorScorePenaltyRatio_position = 2.25;

    double maxOffPenaltyRatio = 1.6;
    double minOffPenaltyratio = .4;
//    double maxMatchPenaltyRatio = 1.1;
    double scalingCheckMissedMatchPenalty = .5;

//    double offToLeftWithTopPeakSpecialCaseCompensate = -.02;
    double offVectorAlreadyMatchedPenaltyRatio = .2;
    double inVectorAlreadyMatchedPenaltyRatio = .2;
    double matchVectorAlreadyMatchedPenaltyRatio = .05;

    double missedPeakDenominator = 3;
    double missedPeakPenaltyRatio = 5;

    // apply scores
    double scorePenalty = 0;
    double sumSpecInWindow = 0;
    double sumAveragInWindow = 0;

    double max_averagine_peak_intensity = averagDist[maxAveragPeakIndex]*overlapObject.getScaleFact();
    double next_to_max_averagine_peak_intensity = averagDist[maxAveragPeakIndex + 1]*overlapObject.getScaleFact(); //close enough

    // contribution to score based on unaccounted peaks
    for (int ov_index = 0; ov_index < (int)offVector.size(); ov_index++) {
        int off_spec_point_index = offVector[ov_index];
        double off_point_scalefactor = find_penalty_scale_factor_if_accounted_for(test_parameters, off_spec_point_index,
                                                                                  offVectorAlreadyMatchedPenaltyRatio);
        double specPoint_y = specPoints[off_spec_point_index].y();
        if (specPoint_y > next_to_max_averagine_peak_intensity*minOffPenaltyratio && specPoint_y <
            max_averagine_peak_intensity*maxOffPenaltyRatio && test_parameters.charge_check == true) {
            sumSpecInWindow += off_point_scalefactor*offVectorScoreDenominator_position*specPoint_y*specPoint_y;
            scorePenalty += off_point_scalefactor*offVectorScorePenaltyRatio_position
                            *offVectorScoreDenominator_position*specPoint_y*specPoint_y;
        } else {
            sumSpecInWindow += off_point_scalefactor*offVectorScoreDenominator_position*max_averagine_peak_intensity;
            scorePenalty += off_point_scalefactor*offVectorScorePenaltyRatio_position
                            *offVectorScoreDenominator_position*max_averagine_peak_intensity;
        }
    }

    // contribution to score based on lesser matching peaks
    for (int iv_index = 0; iv_index < (int)inVector.size(); iv_index++) {
        int in_spec_point_index = inVector[iv_index];
        double in_point_scalefactor = find_penalty_scale_factor_if_accounted_for(test_parameters, in_spec_point_index,
                                                                                  inVectorAlreadyMatchedPenaltyRatio);
        double specPoint_y = specPoints[inVector[iv_index]].y();
        double specPoint_y_squared = specPoint_y*specPoint_y;
       if (specPoint_y > next_to_max_averagine_peak_intensity*minOffPenaltyratio && specPoint_y <
            max_averagine_peak_intensity*maxOffPenaltyRatio) {
            sumSpecInWindow += in_point_scalefactor*inVectorScoreDenominator_position*specPoint_y*specPoint_y;
            scorePenalty += in_point_scalefactor*inVectorScorePenaltyRatio_position*
                            inVectorScoreDenominator_position*specPoint_y_squared;
        } else {
            sumSpecInWindow += in_point_scalefactor*inVectorScoreDenominator_position*max_averagine_peak_intensity;
            scorePenalty += in_point_scalefactor*inVectorScorePenaltyRatio_position*
                            inVectorScoreDenominator_position*max_averagine_peak_intensity;
        }
    }

    // contribution to score based on matching peaks
    for (int mv_index = 0; mv_index < (int)matchVector.size(); mv_index++) {
        double specPoint_y = specPoints[matchVector[mv_index].index].y();
        double specPoint_x = specPoints[matchVector[mv_index].index].x();
        double averagPoint_y = isoFunc.evaluate(specPoint_x, 1);
        double averagPoint_y_squared = averagPoint_y*averagPoint_y;
//        double specPoint_y_squared = specPoint_y*specPoint_y;

        int match_spec_point_index = matchVector[mv_index].index;
        double match_point_scalefactor = find_penalty_scale_factor_if_accounted_for(test_parameters, match_spec_point_index,
                                                                                    matchVectorAlreadyMatchedPenaltyRatio);
        double normalization_ratio = (specPoint_y / averagPoint_y) < (averagPoint_y / specPoint_y) ? (specPoint_y / averagPoint_y)*
                      (specPoint_y / averagPoint_y) : (averagPoint_y / specPoint_y)*(averagPoint_y / specPoint_y);
        sumSpecInWindow += matchVectorScoreDenominator_position*
                           match_point_scalefactor*averagPoint_y_squared;
        scorePenalty += matchVectorScorePenaltyRatio_position*matchVectorScoreDenominator_position*
                        match_point_scalefactor*normalization_ratio*(specPoint_y - averagPoint_y)*(specPoint_y - averagPoint_y);
    }

    // contribution to score based on how well peaks that match accounted for all expected peaks in test
    double sqrt_norm_value = 1;
    if (averagDist.size() > 0) {
        sqrt_norm_value = 1.0 / sqrt((double)averagDist.size());
    }

    // this has changed to fix a error I made where the sqrt norm value is meant to decrease the score for candidates
    // that hit a lot of experimental peaks.  This is a just another part of the scoring that only shows up here
    for (int ad_index = 0; ad_index < (int)averagDist.size(); ad_index++) {
        double averagMiss = averagDist[ad_index];
        double averagPeakMultiplicationValue = scaleFact*scaleFact*averagMiss*averagMiss;

        if (test_parameters.charge_check == true) {
            sumAveragInWindow += missedPeakDenominator*averagPeakMultiplicationValue*averagHitVector.size()
                                 *sqrt_norm_value;
            if (std::binary_search(averagHitVector.begin(), averagHitVector.end(), ad_index) == true) {
                continue;
            } else {
                scorePenalty += missedPeakPenaltyRatio*missedPeakDenominator*averagPeakMultiplicationValue;
            }
        } else {
            sumAveragInWindow += scalingCheckMissedMatchPenalty*missedPeakDenominator*averagPeakMultiplicationValue*
                                 averagHitVector.size()*sqrt_norm_value;
            if (std::binary_search(averagHitVector.begin(), averagHitVector.end(), ad_index) == true) {
                continue;
            } else {
                scorePenalty += scalingCheckMissedMatchPenalty*missedPeakDenominator*averagPeakMultiplicationValue;
            }
        }
    }

    double overlapScore = scorePenalty / (sumSpecInWindow + sumAveragInWindow + 1e-15);
    overlapObject.score = overlapScore;
    overlapObject.matchVector = matchVector;
}

/*!
 * \brief Finds the scale factor that results in the lowest overlap score
 * \param ms1Centroided is input spectra
 * \param averagIsoDist initializes averagine function
 * \param maxPointTest is the peak in the peakTagList to explain
 * \param zCharge initializes averagine function
 * \param mzOffset initializes averagine function
 * \param overlapObject is the output monoisotope
 */
PMI_COMMON_CORE_MINI_EXPORT  void findScaleFactor(const PlotBase & ms1Centroided, const AveragineIsotopeTable & averagIsoDist,
                 TestParameters & test_parameters, point2d maxPointTest, int zVal, double mzOffset,
                 ChargeScoreInfo & overlapObject)
{
    overlapObject.scaleFact = -1;
    if (zVal < 1) {
        debugCoreMini() << "charge values less than 1 not permitted";
        return;
    }
    double maxPointNaiveIntense = maxPointTest.y();
    const std::vector<double> & averagDist = averagIsoDist.isotopeIntensityFractions(mzOffset*zVal);
    int maxAveragPeakIndex = findMaxIndex(averagDist);
    double maxAveragPeakIntens = averagDist[maxAveragPeakIndex];
    double scaleFactNaive = maxPointNaiveIntense / (maxAveragPeakIntens + cTinyNumber);

    ChargeScoreInfo overlapObject_SF = overlapObject;
    overlapObject_SF.scaleFact = scaleFactNaive;
    calcOverlapScore(ms1Centroided, averagIsoDist, test_parameters, overlapObject_SF);
    overlapObject = overlapObject_SF;
}

/*!
 * \brief Finds the scale factor that results in the lowest overlap score by searching linearly through space
 * \param ms1Centroided is input spectra
 * \param averagIsoDist initializes averagine function
 * \param maxPointTest is the peak in the peakTagList to explain
 * \param zCharge initializes averagine function
 * \param mzOffset initializes averagine function
 * \param overlapObject is the output monoisotope
 */
PMI_COMMON_CORE_MINI_EXPORT  void searchFineScaleFactor(const PlotBase & ms1Centroided, double start_ratio,
                                                  double end_ratio, double interval,
                      TestParameters & test_parameters, ChargeScoreInfo & overlapObject)
{
    //test_parameters.scaling_check = true;

    QVector<ChargeScoreInfo> overlapObject_FSF_s;
    ChargeScoreInfo overlapObject_FSF = overlapObject;
    double scaleFactNaive = overlapObject.getScaleFact();

    for (double scaleFact_fact = start_ratio; scaleFact_fact < end_ratio; scaleFact_fact = scaleFact_fact*interval) {
        double scaleFactTest = scaleFactNaive*scaleFact_fact;
        overlapObject_FSF.scaleFact = scaleFactTest;
        calcOverlapScore(ms1Centroided, g_averagIsoDist, test_parameters, overlapObject_FSF);
        overlapObject_FSF_s.push_back(overlapObject_FSF);

    }
    if (overlapObject_FSF_s.size() > 0) { //Out of bounds error check.
        qSort(overlapObject_FSF_s.begin(), overlapObject_FSF_s.end(), lessScore);
        overlapObject = overlapObject_FSF_s[0];
    } else {
        return;
    }
}

/*!
 * \brief Finds the scale factor that results in the lowest overlap score by breaking space in binary fashion
 * \param ms1Centroided is input spectra
 * \param averagIsoDist initializes averagine function
 * \param maxPointTest is the peak in the peakTagList to explain
 * \param zCharge initializes averagine function
 * \param mzOffset initializes averagine function
 * \param overlapObject is the output monoisotope
 */
PMI_COMMON_CORE_MINI_EXPORT  void searchFineScaleFactorBinary(const PlotBase & ms1Centroided, double start_ratio,
                                                        double end_ratio, int number_checks,
                                                        TestParameters & test_parameters, ChargeScoreInfo & overlapObject)
{
    //test_parameters.scaling_check = true;
    double scaleFactNaive = overlapObject.getScaleFact();
    double upper_bound = start_ratio*scaleFactNaive;
    double lower_bound = end_ratio*scaleFactNaive;
    double current_middle = scaleFactNaive;

    int checks = 0;
    while (checks < number_checks) {
        double check_below_scaleFact = (lower_bound + current_middle) / 2;
        double check_above_scaleFact = (upper_bound + current_middle) / 2;

        ChargeScoreInfo overlapObject_below = overlapObject;
        ChargeScoreInfo overlapObject_above = overlapObject;

        overlapObject_below.scaleFact = check_below_scaleFact;
        calcOverlapScore(ms1Centroided, g_averagIsoDist, test_parameters, overlapObject_below);

        overlapObject_above.scaleFact = check_above_scaleFact;
        calcOverlapScore(ms1Centroided, g_averagIsoDist, test_parameters, overlapObject_above);

        if (overlapObject_above.score < overlapObject_below.score) {
            double current_middle_temp = (current_middle + upper_bound) / 2;
            lower_bound = current_middle;
            current_middle = current_middle_temp;
        }
        else {
            double current_middle_temp = (current_middle + lower_bound) / 2;
            upper_bound = current_middle;
            current_middle = current_middle_temp;
        }
        checks++;
    }
    overlapObject.scaleFact = current_middle;
    calcOverlapScore(ms1Centroided, g_averagIsoDist, test_parameters, overlapObject);
}

/*!
 * \brief Finds the mz position of the isotope within mzStepSize precision / 2 searching linearly
 * \param ms1Centroided is input spectra
 * \param averagIsoDist initializes isoFunc
 * \param leftBoundOff sets starting search position of mono mz from initial val
 * \param rightBoundOff sets ending search position of mono mz from initial val
 * \param mzStepSize is resolution of the search
 * \param overlapObject initializes isoFunc, also output
 */
PMI_COMMON_CORE_MINI_EXPORT  void searchFinePosition(const PlotBase & ms1Centroided, double leftBoundOff, double rightBoundOff,
                 double mzStepSize, TestParameters & test_parameters, ChargeScoreInfo & overlapObject)
{
    if (mzStepSize > 0 && (leftBoundOff + rightBoundOff > 0)) {

        const point2dList & specPoints = ms1Centroided.getPointList(); // should limit to appropriate range

        double mzCenterOffset = overlapObject.mzOffset;
        double startMz = mzCenterOffset - leftBoundOff;
        double endMz = mzCenterOffset + rightBoundOff;

        const std::vector<double> & averagDist = g_averagIsoDist.isotopeIntensityFractions(overlapObject.getMzOffset()*overlapObject.getCharge());
        AveragineIsotopeFunction isoFunc(averagDist);

        double rightSideFudge = findStdDev(test_parameters.uncertainty, mzCenterOffset, test_parameters.uncertainty_scaling_type);
        double leftSideFudge = findStdDev(test_parameters.uncertainty, mzCenterOffset, test_parameters.uncertainty_scaling_type);
        isoFunc.setScaleFact(overlapObject.scaleFact);
        isoFunc.setCharge(overlapObject.charge);

        PlotBase xcorrValues;
        //PlotBase testcorrValues;
        ChargeScoreInfo overlapObject_P = overlapObject;
        for (double mzOffsetCheck = startMz; mzOffsetCheck < endMz; mzOffsetCheck = mzOffsetCheck + mzStepSize) {
            isoFunc.setMzOffset(mzOffsetCheck);
            isoFunc.setStdDev(test_parameters.uncertainty, test_parameters.uncertainty_scaling_type);
            double xcorrSum = 0;
            // double testcorrSum = 0;

            // get the start and end values, cause mixmatch if both out of bounds
            int xcorrSpecWindowIdx_start = ms1Centroided.getIndexLessOrEqual(mzOffsetCheck - leftSideFudge, false) + 1;
            int xcorrSpecWindowIdx_end = ms1Centroided.getIndexLessOrEqual(mzOffsetCheck + averagDist.size()
                                                                           / overlapObject.getCharge() + rightSideFudge, false);
            if (xcorrSpecWindowIdx_end < xcorrSpecWindowIdx_start)
                continue;
            for (int k = xcorrSpecWindowIdx_start; k <= (int)xcorrSpecWindowIdx_end; k++) {
                const point2d & p = specPoints[k];
                double specPoint_y = p.y();
                double specPoint_x = p.x();
                double averagPoint_y = isoFunc.evaluate(specPoint_x, 3); // 3 standard deviations
                xcorrSum +=  specPoint_y*averagPoint_y;

            }
            xcorrValues.addPoint(mzOffsetCheck, xcorrSum);
            overlapObject_P.mzOffset = mzOffsetCheck;
            // calcOverlapScore(ms1Centroided, g_averagIsoDist, test_parameters, overlapObject_P);
            // testcorrValues.addPoint(mzOffsetCheck, overlapObject_P.getScore()); // for testing

        }
        // testcorrValues.sortPointListByY(true);
        xcorrValues.sortPointListByY(false);
        if (xcorrValues.getPointListSize() > 0) {
            // double topPeakMz = testcorrValues.getPoint(testcorrValues.getPointListSize() - 1 ).x();
            double topPeakMz = xcorrValues.getPoint(0).x();
            overlapObject.mzOffset = topPeakMz;
        } else {
            overlapObject.score = cErrorScore;
        }
    } else {
        overlapObject.score = cErrorScore;
    }
}


/*!
 * \brief Converts the charge and maxima peak into an mzOffset to trigger a search for mono position
 * \param ms1Centroided is input spectra
 * \param averagIsoDist initializes averagine function
 * \param maxPointTest is the peak in the peakTagList to explain
 * \param zVal initializes averagine function
 * \param overlapObject the object passed around that holds onto the best values returned back
 * \param initial whether the search is for a mono position determined outside this file
 */
PMI_COMMON_CORE_MINI_EXPORT  void findMzOffset(const PlotBase & ms1Centroided, const AveragineIsotopeTable & averagIsoDist,
             TestParameters & test_parameters, point2d & maxPointTest, int zVal, ChargeScoreInfo & overlapObject)
{
    double maxPointTest_Mz = maxPointTest.x();
    const std::vector<double> & averagDist = averagIsoDist.isotopeIntensityFractions(maxPointTest_Mz*zVal);
    int maxAveragPeakIndex = findMaxIndex(averagDist);
    double mzOffset = maxPointTest_Mz - maxAveragPeakIndex*cPeakSpacing / zVal;

    overlapObject.mzOffset = mzOffset;
    if (overlapObject.mzOffset <= std::numeric_limits<int>::min())
        debugCoreMini() << "mzOffset is incorrectly assigned";
    findScaleFactor(ms1Centroided, averagIsoDist, test_parameters, maxPointTest, zVal, mzOffset, overlapObject);
}

// this function compares different candiate isotope patterns a very nasty spectrum that both got
// terrible scores and sees which hit more peaks --  in bad enough data this is all we have to go on
PMI_COMMON_CORE_MINI_EXPORT  bool lessHitRatio(const ChargeScoreInfo &overlapObject1, const ChargeScoreInfo &overlapObject2)
{
    double mass_1 = overlapObject1.getMzOffset()*overlapObject1.getCharge();
    const std::vector<double> & averagDist1 = g_averagIsoDist.isotopeIntensityFractions(mass_1);
    int averagDistSize1 = static_cast<int>(averagDist1.size());
    int overlapObjectMatchVectorSize1 = static_cast<int>(overlapObject1.getMatchVector().size());
    double hit_ratio_1 = 0;
    if (averagDistSize1 > 0) {
        hit_ratio_1 = (double)overlapObjectMatchVectorSize1 / averagDistSize1;
    }

    double mass_2 = overlapObject2.getMzOffset()*overlapObject2.getCharge();
    const std::vector<double> & averagDist2 = g_averagIsoDist.isotopeIntensityFractions(mass_2);
    int averagDistSize2 = static_cast<int>(averagDist2.size());
    int overlapObjectMatchVectorSize2 = static_cast<int>(overlapObject2.getMatchVector().size());
    double hit_ratio_2 = 0;
    if (averagDistSize2 > 0) {
        hit_ratio_2 = (double)overlapObjectMatchVectorSize2 / averagDistSize2;
    }

    return hit_ratio_1 < hit_ratio_2;
}

const static double cMinScoreThresholdToSearchAlternatePositions = 1.4;
const static double cMinMassToSearchAlternatePositions = 1100.0;
const static double cMaxMassToSearchAlternatePositions = 4200.0;

/*!
 * \brief Finds the charge that results in the lowest match score
 * \param ms1Centroided is input spectra
 * \param averagIsoDist initializes averagine function
 * \param maxPointTest is the peak in the peakTagList to explain
 * \param overlapObject the object passed around that holds onto the best values returned back
 * \param initial whether the search is for a mono position determined outside this file
 */
PMI_COMMON_CORE_MINI_EXPORT  void searchCharge(const PlotBase & ms1Centroided, const AveragineIsotopeTable & averagIsoDist,
             TestParameters & test_parameters, point2d & maxPointTest, ChargeScoreInfo & overlapObject)
{
    overlapObject.charge = -1;
    test_parameters.charge_check = true;
    std::vector<ChargeScoreInfo> overlapObject_C_s;
    overlapObject_C_s.reserve(test_parameters.charge_end - test_parameters.charge_start + 1);
    for (int zVal = test_parameters.charge_start; zVal <= test_parameters.charge_end; zVal++) {
        ChargeScoreInfo overlapObject_C;
        overlapObject_C.charge = zVal;
//        double current_mass = maxPointTest.x()*zVal;

        findMzOffset(ms1Centroided, averagIsoDist, test_parameters, maxPointTest, zVal, overlapObject_C);
        if (overlapObject_C.getScore() < cMinScoreThresholdToSearchAlternatePositions) {
            searchFineScaleFactorBinary(ms1Centroided, .5, 2.0, 10, test_parameters, overlapObject_C);
        }
        overlapObject_C_s.push_back(overlapObject_C);
        double lrFudge = findStdDev(test_parameters.uncertainty, maxPointTest.x(), test_parameters.uncertainty_scaling_type)*2;
        double positionToTry = maxPointTest.x() + 1.0 / zVal;
        double leftBound = positionToTry - lrFudge;
        double rightBound = positionToTry + lrFudge;
        point2d bestPointInBounds;
        bool valid = false;
        getMostIntensePointWithinBound(ms1Centroided, leftBound, rightBound, bestPointInBounds,
                                       &valid);
        if (valid == true) {
            findMzOffset(ms1Centroided, averagIsoDist, test_parameters, bestPointInBounds, zVal, overlapObject_C);
            if (overlapObject_C.getMzOffset() < maxPointTest.x() + 1.4*lrFudge) {
                if (overlapObject_C.getScore() < cMinScoreThresholdToSearchAlternatePositions) {
                    searchFineScaleFactorBinary(ms1Centroided, .4, 2.0, 10, test_parameters, overlapObject_C);
                }
                overlapObject_C_s.push_back(overlapObject_C);
            } // this is slow, but OK for now
        }
        double positionToTryLeft = maxPointTest.x() - 1.0 / zVal;
        double leftBoundLeft = positionToTryLeft - lrFudge;
        double rightBoundLeft = positionToTryLeft + lrFudge;
        point2d bestPointInBoundsLeft;
        bool validLeft = false;
        getMostIntensePointWithinBound(ms1Centroided, leftBoundLeft, rightBoundLeft,
                                       bestPointInBoundsLeft, &validLeft);
        if (validLeft == true) {
            findMzOffset(ms1Centroided, averagIsoDist, test_parameters, bestPointInBoundsLeft, zVal, overlapObject_C);
            if (overlapObject_C.getScore() < cMinScoreThresholdToSearchAlternatePositions) {
                searchFineScaleFactorBinary(ms1Centroided, .4, 2.0, 10, test_parameters, overlapObject_C);
            }
            overlapObject_C_s.push_back(overlapObject_C);
        }
        //}
    }

    // sort by score ascending
    qSort(overlapObject_C_s.begin(), overlapObject_C_s.end(), lessScore);

    // if all the scores are bad, choose the one that hits the highest percentage of peaks
    if (overlapObject_C_s.size() > 0) {
        overlapObject = overlapObject_C_s[0];
        if (overlapObject.getScore() > 1.6) {
            qSort(overlapObject_C_s.rbegin(), overlapObject_C_s.rend(), lessHitRatio);
            overlapObject = overlapObject_C_s[0];
        }
    }
}

// determine whether object 1 is better than object 2.  This is overly complicated I know, but it's based off empiracle comparison
// this is very unstable
PMI_COMMON_CORE_MINI_EXPORT  bool compareNearMonos(ChargeScoreInfo & overlapObject1, ChargeScoreInfo & overlapObject2, int upOrDown) {

    // check if one didn't even match to anything
    if (overlapObject1.getMatchVector().size() <= 0) {
        return false;
    }
    if (overlapObject2.getMatchVector().size() <= 0) {
        return true;
    }

    // uncomment to simplify things.  So far still worse
    //if (overlapObject1.score < overlapObject2.score) {
    //    return true;
    //}
    ////else {
    //    return false;
    ////}

    // if going down
    if (overlapObject1.score < overlapObject2.score && overlapObject1.scaleFact >
        1.3*overlapObject2.scaleFact) {
        return true;
    }
    if (upOrDown == -1) {
        if (overlapObject1.getMatchVector()[0].index < overlapObject2.getMatchVector()[0].index &&
                 overlapObject1.getMatchVector()[0].percent_account_for > .5 &&
                 overlapObject1.score < 1.5*overlapObject2.score) {
            return true;
        }
        return false;
    }

    // if going up
    if (upOrDown == 1) {
        if (overlapObject1.getMzOffset()*overlapObject1.getCharge() < 4200 &&
                 overlapObject1.getMatchVector()[0].index == overlapObject2.getMatchVector()[0].index &&
                 overlapObject1.score < 1.25*overlapObject2.score) {
            return true;
        }
        else if (overlapObject1.getMzOffset()*overlapObject1.getCharge() > 4200 &&
                 overlapObject1.score < .6*overlapObject2.score) {
            return true;
        }

        return false;
    }
    return false; //sanity check
}

/*!
 * \brief Finds the max peak that should be tallest in distribution, peaks needs to exist
 * \param ms1Centroided is input spectra
 * \param averagIsoDist initializes averagine function
 * \param overlapObject the object passed around that holds onto the best values returned back
 * \param initial whether the search is for a mono position determined outside this file
 */
PMI_COMMON_CORE_MINI_EXPORT  ChargeScoreInfo searchMaxPeak(const PlotBase & ms1Centroided,
                                                     const AveragineIsotopeTable & averagIsoDist,
                                                     TestParameters & test_parameters, point2d & maxPointTest)
{
    ChargeScoreInfo overlapObject;
    test_parameters.scaling_check = false;
    searchCharge(ms1Centroided, averagIsoDist, test_parameters, maxPointTest, overlapObject);

    // find score for initial check without charge_check
    test_parameters.charge_check = false;
    findMzOffset(ms1Centroided, averagIsoDist, test_parameters, maxPointTest, overlapObject.getCharge(), overlapObject);
    searchFineScaleFactorBinary(ms1Centroided, .5, 2.0, 10, test_parameters, overlapObject);
    calcOverlapScore(ms1Centroided, averagIsoDist, test_parameters, overlapObject);

    // find correct range of off by x
    int initCharge = overlapObject.charge;
    const std::vector<double> & averagDist = averagIsoDist.isotopeIntensityFractions(maxPointTest.x()*overlapObject.charge);
    double intens_peak_offset = (double)findMaxIndex(averagDist) / overlapObject.charge;
    double initMaxMz = overlapObject.mzOffset + intens_peak_offset;
    int averagLength = static_cast<int>(averagDist.size());
    int averagMaxValIndex = findMaxIndex(averagDist);

    int searchDown = (int)(averagMaxValIndex - averagLength);
    int searchUp = (int)averagMaxValIndex;
    double lrFudge = findStdDev(test_parameters.uncertainty, initMaxMz, test_parameters.uncertainty_scaling_type)*2;

    // search left (decreasing in mz), stop if doesn't improve, revert if keeps improving past last possible mono-peak
    ChargeScoreInfo bestMonoFinal_down = overlapObject;
    ChargeScoreInfo bestMonoFinal_up;
    int best_peak = 0;
    int best_peak_previous = -1;
    int counts_no_change_best_peak = 0;
    for (int peakToCheckNumber = -1; peakToCheckNumber >= searchDown - 1; peakToCheckNumber--) {
        double positionToTry = initMaxMz + (double)peakToCheckNumber / initCharge; // something is up here...
        double leftBound = positionToTry - lrFudge;
        double rightBound = positionToTry + lrFudge;
        ChargeScoreInfo bestMonoCurrent;
        bestMonoCurrent.charge = initCharge;
        point2d bestPointInBounds;
        bool valid = false;
        getMostIntensePointWithinBound(ms1Centroided, leftBound, rightBound, bestPointInBounds,
                                       &valid);
        if (valid == false) {
            //break;
            bestPointInBounds.setX(positionToTry);
            bestPointInBounds.setY(maxPointTest.y()); // this could be better
        } //else {
        findMzOffset(ms1Centroided, averagIsoDist, test_parameters, bestPointInBounds, bestMonoFinal_down.charge, bestMonoCurrent);

        searchFineScaleFactorBinary(ms1Centroided, .5, 2.0, 10, test_parameters, bestMonoCurrent);
        calcOverlapScore(ms1Centroided, averagIsoDist, test_parameters, bestMonoCurrent);
        if (compareNearMonos(bestMonoCurrent, bestMonoFinal_down, -1)) {
            best_peak = peakToCheckNumber;
            bestMonoFinal_down = bestMonoCurrent;
        } else {
            break;
        }
        //}
        if (best_peak == best_peak_previous) {
            best_peak = best_peak_previous;
            counts_no_change_best_peak++;
            if (counts_no_change_best_peak >= 2) {
                break;
            }
        }
        // if the score has kept improving past theoretical bounds (into convoluting one)
        if (best_peak == searchDown - 1) {
            bestMonoFinal_down = overlapObject; // this is dangerous, ie, this mono probably won;t be found
            break;
        }
    }
    // search right (increasing in mz), stop if doesn't improve, revert if keeps improving past last possible mono-peak
    bestMonoFinal_up = bestMonoFinal_down;
    best_peak = 0;
    best_peak_previous = -1;
    counts_no_change_best_peak = 0;
    for (int peakToCheckNumber = 1; peakToCheckNumber <= searchUp + 1; peakToCheckNumber++) {
        double positionToTry = initMaxMz + (double)peakToCheckNumber / initCharge;
        double leftBound = positionToTry - lrFudge;
        double rightBound = positionToTry + lrFudge;
        ChargeScoreInfo bestMonoCurrent;
        bestMonoCurrent.charge = initCharge;
        point2d bestPointInBounds;
        bool valid = false;
        getMostIntensePointWithinBound(ms1Centroided, leftBound, rightBound, bestPointInBounds,
                                       &valid);
        if (valid == false) {
            //break;
            bestPointInBounds.setX(positionToTry);
            bestPointInBounds.setY(maxPointTest.y());
        } //else {
        findMzOffset(ms1Centroided, averagIsoDist, test_parameters, bestPointInBounds, bestMonoFinal_up.charge, bestMonoCurrent);
        //searchFineScaleFactor(ms1Centroided, .5, 2.0, 1.2, test_parameters, bestMonoCurrent);
        searchFineScaleFactorBinary(ms1Centroided, .5, 2.0, 10, test_parameters, bestMonoCurrent);
        calcOverlapScore(ms1Centroided, averagIsoDist, test_parameters, bestMonoCurrent);
//        bool valid2 = false;
        // getMostIntensePointWithinBound(ms1Centroided, bestMonoCurrent.getMzOffset() - lrFudge, bestMonoCurrent.getMzOffset() + lrFudge, bestPointInBounds, valid2);
        //if (bestMonoCurrent.score < bestMonoFinal_up.score || (bestMonoCurrent.score < bestMonoFinal_up.score + .01 && valid)) {
        if (compareNearMonos(bestMonoCurrent, bestMonoFinal_up, 1)) {
            best_peak = peakToCheckNumber;
            bestMonoFinal_up = bestMonoCurrent;
        } else {
            break;
        }
        //}
        if (best_peak == best_peak_previous) {
            best_peak = best_peak_previous;
            counts_no_change_best_peak++;
            if (counts_no_change_best_peak >= 2) {
                break;
            }
        }
        // if the score has kept improving past theoretical bounds (into convoluting one)
        if (peakToCheckNumber == searchUp + 1) {
            bestMonoFinal_up = bestMonoFinal_down;
            break;
        }
    }
    // add in the actual points from indicies
    std::vector<PointAndPercentAccountFor> best_mono_final_up_match_vector = bestMonoFinal_up.getMatchVector();
    for (int bmfmv_index = 0; bmfmv_index < (int)best_mono_final_up_match_vector.size(); bmfmv_index++) {
        PointAndPercentAccountFor & mono_partially_acounted_for_point_percent = best_mono_final_up_match_vector[bmfmv_index];
        point2d mono_partially_acounted_for_mono_point = ms1Centroided.getPoint(mono_partially_acounted_for_point_percent.index);
        if (mono_partially_acounted_for_point_percent.percent_account_for > 1) {
            bestMonoFinal_up.matchPointsVector.push_back(mono_partially_acounted_for_mono_point);
        } else {
            bestMonoFinal_up.matchPointsVector.push_back(point2d(mono_partially_acounted_for_mono_point.x(), mono_partially_acounted_for_mono_point.y()*
                                                         mono_partially_acounted_for_point_percent.percent_account_for));
        }
    }
    return bestMonoFinal_up;
}

PMI_COMMON_CORE_MINI_EXPORT  bool charge_mz_Less(const ChargeScoreInfo & mono1, const ChargeScoreInfo & mono2)
{
    if (mono1.getCharge() == mono2.getCharge()) {
        return (mono1.getMzOffset() < mono2.getMzOffset());
    }
    return (mono1.getCharge() < mono2.getCharge());
}

PMI_COMMON_CORE_MINI_EXPORT  bool monoMzLess(const ChargeScoreInfo & mono1, const ChargeScoreInfo & mono2)
{
    return (mono1.getMzOffset() < mono2.getMzOffset());
}

PMI_COMMON_CORE_MINI_EXPORT  bool chargeLess(const ChargeScoreInfo & mono1, const ChargeScoreInfo & mono2)
{
    return (mono1.getCharge() < mono2.getCharge());
}

PMI_COMMON_CORE_MINI_EXPORT  bool scoreLess(const ChargeScoreInfo & mono1, const ChargeScoreInfo & mono2)
{
    return (mono1.getScore() < mono2.getScore());
}

PMI_COMMON_CORE_MINI_EXPORT  bool percentIonCurrentGreater(const ChargeScoreInfo & mono1, const ChargeScoreInfo & mono2)
{
    return (mono1.getPercentIonCurrent() > mono2.getPercentIonCurrent());
}

PMI_COMMON_CORE_MINI_EXPORT  bool areUnique(const TestParameters & test_parameters, const ChargeScoreInfo & mono1,
                                      const ChargeScoreInfo & mono2)
{
    double uncertainty = findStdDev(test_parameters.uncertainty, mono1.getMzOffset(), test_parameters.uncertainty_scaling_type);
    if (mono1.getMzOffset() > mono2.getMzOffset() + uncertainty || mono1.getMzOffset() <
        mono2.getMzOffset() - uncertainty || mono1.getCharge() != mono2.getCharge()) {
        return true;
    }
    return false;
}

PMI_COMMON_CORE_MINI_EXPORT  void sortByMzForCharge(QVector<ChargeScoreInfo> & bestMonos)
{
    if (bestMonos.size() <= 0)
        return;

    int current_charge = bestMonos[0].getCharge();
    int starting_index = 0;
    for (int bm_index = 1; bm_index < (int)bestMonos.size(); bm_index++) {
        int next_charge = bestMonos[bm_index].getCharge();
        if (next_charge != current_charge) {
            current_charge = next_charge;
            qSort(bestMonos.begin() + starting_index, bestMonos.begin() + bm_index - 1, monoMzLess);
            starting_index = bm_index;
        }
    }
    if (starting_index == (int)bestMonos.size() - 1)
        return;
    qSort(bestMonos.begin() + starting_index, bestMonos.end(), monoMzLess);
}

/*!
 * \brief checks to see if the mono call has already been made in the list (previous better, unfortunate but unavoidable that
 * \this still happens
 * \param bestMonos previously added monos, from taller peaks
 * \param bestMono mono to check
 */
PMI_COMMON_CORE_MINI_EXPORT  bool isUniqueInMonoList(QVector<ChargeScoreInfo> & bestMonos, ChargeScoreInfo & bestMono)
{
    for (int i = 0; i < bestMonos.size(); i++) { // ***
        if (pmi::fsame(bestMonos[i].getMzOffset(), bestMono.getMzOffset(), 1e-6) && bestMonos[i].getCharge() == bestMono.getCharge()) {
            return false;
        }
    }
    return true;
}

/*!
 * \brief Cycles through points in window, and asigns monos that best explain all (greedy)
 * \param ms1Centroided is spectrum points
 * \param averagIsoDist initializes isoFunc
 * \param specWindowMZ_start beginning of mz isolation window
 * \param specWindowMZ_end end of mz isolation window
 * \param bestMonos list of mono / isotope distribution objects that passed criteria
 */
PMI_COMMON_CORE_MINI_EXPORT  void eliminateTagListPeaks(
                      const PlotBase & ms1Centroided, double specWindowMZ_start, double specWindowMZ_end,
                      double total_ion_current, double percent_current_allowed, TestParameters &test_parameters,
                      QVector<ChargeScoreInfo> & bestMonos)
{
    bestMonos.clear();
    if (total_ion_current <= 0) {
        return;
    }
    const point2dList specPointsInWindow = ms1Centroided.getDataPoints(specWindowMZ_start,
                                                                       specWindowMZ_end, true);
    if (specPointsInWindow.size() <= 0) {
        return; // protects against windowpoint size == 0
    }
    // create list of points in the window
    test_parameters.current_match_list.addPoints2dList(specPointsInWindow);
    // set index bounds
    int windowSpace_LeftOffset = ms1Centroided.getIndexLessOrEqual(specWindowMZ_start);
    if (test_parameters.current_match_list.getWindowPoint(0).x() != specWindowMZ_start) {
       windowSpace_LeftOffset += 1;
    }
    test_parameters.current_match_list.setWindowPeakOffset(windowSpace_LeftOffset);
    // explain peaks by monos, starting with tallest peaks until all are explained
    for (int windowPointIndex = 0; test_parameters.current_match_list.checkAllPointsExcluded() == false && windowPointIndex <
         test_parameters.current_match_list.getSize(); windowPointIndex = windowPointIndex + 1) {
        if (test_parameters.current_match_list.checkPointExcludedSorted(windowPointIndex) == false) {
            point2d maxPeakNaive = test_parameters.current_match_list.getWindowPointSorted(windowPointIndex);
            // get best mono
            ChargeScoreInfo bestMono = searchMaxPeak(ms1Centroided, g_averagIsoDist, test_parameters, maxPeakNaive);
            if (!isUniqueInMonoList(bestMonos, bestMono))  {
                continue;
            }
            bestMono.percent_ion_current = bestMono.getPointsIntensBetweenIndices(test_parameters, windowSpace_LeftOffset,
                                                      windowSpace_LeftOffset + static_cast<int>(specPointsInWindow.size())) / total_ion_current;

            // exclude the points explained by mono from subsequent searches
            const std::vector<PointAndPercentAccountFor> & currMatchList = bestMono.matchVector;
            for (int j = 0; j < (int)currMatchList.size(); j++) {
                PointAndPercentAccountFor currMatch = currMatchList[j];
                if (currMatch.percent_account_for > .45) {
                    int windowPointIndex_match = currMatchList[j].index - windowSpace_LeftOffset;
                    if (windowPointIndex_match >= 0 && windowPointIndex_match < test_parameters.current_match_list.getSize()) {
                        test_parameters.current_match_list.excludePoint(windowPointIndex_match);
                    }
                }
            }

            if (bestMono.percent_ion_current > percent_current_allowed / 2) {
                //if (checkIfMonoDuplicate(bestMonos, bestMono))
                bestMonos.push_back(bestMono);
            } else {
                break;
            }
        }
    }

    /*
    qSort(bestMonos.begin(), bestMonos.end(), chargeLess);
    sortByMzForCharge(bestMonos);
    */
    qSort(bestMonos.begin(), bestMonos.end(), charge_mz_Less);

    // removeRedundantsFromSortedVector(test_parameters, bestMonos);
}

/*!
 * \brief check to see if window and uncertainty values permit a valid expeimentla window search
 * \param specWindowMZ_start begin of window
 * \param specWindowMZ_end end of window
 * \param stdDev measure of the uncertainty in isotope peak position
 */
PMI_COMMON_CORE_MINI_EXPORT  bool testFindMonoInput(double specWindowMZ_start, double specWindowMZ_end, double stdDev)
{
    bool true_or_false = true;
    if (specWindowMZ_start > specWindowMZ_end) {
        warningCoreMini() << "spectrum window start and end positions mixmatched";
        true_or_false = false;
    }
    if (stdDev <= 0) {
        warningCoreMini() << "uncertainty in mz less than or equal to 0";
        true_or_false = false;
    }
    return true_or_false;
}

/*!
 * \brief find all mono / isotope distributions in window and convert to monoCandList
 * \param opts optional input parameter overrides
 * \param ms1Original original data format ms1/2 (profile mode or centroided
 * \param ms1Centroided centrioded ms1/2 spectrum data
 * \param specWindowMZ_start beginning of mz isolation window
 * \param specWindowMZ_end end of mz isolation window
 * \param ms1ScanNumberQStr scan number of debug output
 * \param chargeMonosCandList output datastructure for candidate storage, sorted by score
 */
PMI_COMMON_CORE_MINI_EXPORT  void findChargesAndMonos(const ProcessorOptions& opts, const PlotBase & ms1Centroided,
                    double specWindowMZ_start, double specWindowMZ_end, QString ms1ScanNumberStr,
                    double percent_ion_current_allow, QVector<ChargeScoreInfo> & bestMonos)
{
    bestMonos.clear();
    if (testFindMonoInput(opts.po_MinZCheck, opts.po_MaxZCheck, opts.po_uncertainty) == false)
        return;
    TestParameters test_parameters;
    test_parameters.charge_end = opts.po_MaxZCheck;
    test_parameters.uncertainty_scaling_type = opts.po_uncertainty_scaling_type;

    test_parameters.uncertainty = opts.po_uncertainty;
    if (opts.po_uncertainty_scaling_type == SquareRootUncertainty) {
        test_parameters.uncertainty /= sqrt(1000.0); // scale to peak at 1000 Da
    }

    // get all the potential monos that explain exclusive peaks in the window
    double total_ion_current = ms1Centroided.computeSumIntensities(specWindowMZ_start, specWindowMZ_end);
    if (total_ion_current <= 0) {
        return;
    }
    eliminateTagListPeaks(ms1Centroided, specWindowMZ_start, specWindowMZ_end, total_ion_current, percent_ion_current_allow,
                          test_parameters, bestMonos);
    qSort(bestMonos.begin(), bestMonos.end(), percentIonCurrentGreater);
}

/*!
 * \brief find the mono / isotope distributions that best explain the spectrum
 * \param ms1Centroided ms points
 * \param specWindowMZ_start beginning of spectrum bounds
 * \param specWindowMZ_end end of spectrum bounds
 * \param stdDev sqrt uncertainty
 * \param bestMonos
 */
PMI_COMMON_CORE_MINI_EXPORT  void findBestMonos_isomatcher(const PlotBase & ms1Centroided, double specWindowMZ_start, double specWindowMZ_end, double uncertainty,
                   UncertaintyScaling uncertainty_type, QVector<ChargeScoreInfo> & bestMonos)
{

    if (testFindMonoInput(specWindowMZ_start, specWindowMZ_end, uncertainty) == false)
        return;
    double total_ion_current = ms1Centroided.computeSumIntensities(specWindowMZ_start, specWindowMZ_end);
    if (total_ion_current <= 0) {
        return;
    }

    TestParameters test_parameters;
    test_parameters.uncertainty_scaling_type = uncertainty_type;
    test_parameters.uncertainty = uncertainty;
    eliminateTagListPeaks(ms1Centroided, specWindowMZ_start, specWindowMZ_end, total_ion_current, 0, test_parameters, bestMonos);
}

/*!
 * \brief find the mono / isotope distributions that best explain the spectrum
 * \param ms1Centroided ms points
 * \param specWindowMZ_start beginning of spectrum bounds
 * \param specWindowMZ_end end of spectrum bounds
 * \param ppm linear uncertainty
 * \param bestMonos
 */
PMI_COMMON_CORE_MINI_EXPORT  void findBestMonos(const PlotBase & ms1Centroided, double specWindowMZ_start, double specWindowMZ_end,
                   const ProcessorOptions & opts, QVector<ChargeScoreInfo> & bestMonos)
{

    if (testFindMonoInput(specWindowMZ_start, specWindowMZ_end, opts.po_uncertainty) == false)
        return;

    double total_ion_current = ms1Centroided.computeSumIntensities(specWindowMZ_start, specWindowMZ_end);
    if (total_ion_current <= 0) {
        return;
    }

    TestParameters test_parameters;
    test_parameters.uncertainty_scaling_type = opts.po_uncertainty_scaling_type;
    test_parameters.uncertainty = opts.po_uncertainty;
    eliminateTagListPeaks(ms1Centroided, specWindowMZ_start, specWindowMZ_end, total_ion_current, 0, test_parameters, bestMonos);
}

//E.g. "1000,+3,10e+5,.01"
PMI_COMMON_CORE_MINI_EXPORT  QString makeMonoChargeYScaleStdDevToString(double mass_mono_uncharged, int charge, double y_scale, double stdDev)
{
    return QStringLiteral("%1,+%2,%3,%4").arg(QString::number(mass_mono_uncharged,'g',12)).arg(charge).arg(QString::number(y_scale,'e',12)).arg(QString::number(stdDev,'g',12));
}

PMI_COMMON_CORE_MINI_EXPORT  void makeMonoChargeYScaleStdDevFromString(const QString & str, double & mass_mono_uncharged, int & charge, double & y_scale, double & std_dev)
{
    QRegExp rxParse("[\\t ]*([\\d]*\\.[\\d]*|[\\d]+)[\\t ]*,[\\t ]*\\+?([\\d]+)[\\t ]*,[\\t ]*((?:[\\d]*\\.[\\d]*|[\\d]+)e(?:\\+|-)[\\d]+)[\\t ]*,[\\t ]*([\\d]*\\.[\\d]*|[\\d]+)");
    int pos = rxParse.indexIn(str);
    if (pos > -1) {
        QString mass_mono_uncharged_string = rxParse.cap(1);
        mass_mono_uncharged = mass_mono_uncharged_string.toDouble();
        QString charge_string = rxParse.cap(2);
        charge = charge_string.toDouble();
        QString y_scale_string = rxParse.cap(3);
        QRegExp rxNumber("[\\t ]*([\\d]*\\.[\\d]*|[\\d]+)[\\t ]*e(\\+|\\-)[\\t ]*([\\d]+)[\\t ]*");
        int pos_2 = rxNumber.indexIn(y_scale_string);
        if (pos_2 > -1) {
            QString number_string = rxNumber.cap(1);
            QString sign_string = rxNumber.cap(2);
            QString exponent_string = rxNumber.cap(3);
            if (sign_string == "-") {
                y_scale = number_string.toDouble()*pow(10, 1 / exponent_string.toDouble());
            } else {
                y_scale = number_string.toDouble()*pow(10, exponent_string.toDouble());
            }
        }
        QString std_dev_string = rxParse.cap(4);
        std_dev = std_dev_string.toDouble();
    }
}

/*!
 * \brief score a mono / isotope distribution where the scale factor has not been set
 * \param pointsList spectrum points
 * \param stdDev uncertainty / uncertainty tolerance in ppm
 * \param overlapObject return isotope object
 */
PMI_COMMON_CORE_MINI_EXPORT  void testMonoInitial(const PlotBase & pointsList, double ppm, ChargeScoreInfo & overlapObject)
{
    double monoMZ = overlapObject.getMzOffset();
    int charge = overlapObject.getCharge();
    TestParameters test_parameters;
    test_parameters.uncertainty_scaling_type = LinearUncertainty;
    test_parameters.uncertainty = ppm;
    double stdDev = findStdDev(ppm, monoMZ, LinearUncertainty);

    // get the scale factor (TODO: find a better way to do this)
    const std::vector<double> & averagDist = g_averagIsoDist.isotopeIntensityFractions(monoMZ*charge);
    if (averagDist.size() <= 0) {
        warningCoreMini() << "************* averagDist.size() = " << averagDist.size();
        return;
    }

    const point2dList rangePointsList = pointsList.getAreaPointList(monoMZ + findMaxIndex(averagDist) / charge - stdDev,
                                                                    monoMZ + findMaxIndex(averagDist) / charge + stdDev,
                                                                    false, false);
    point2d maxPointSpec = getMaxPoint(rangePointsList);
    double maxPointSpecY = maxPointSpec.y();
    double maxPointAveragY = averagDist[findMaxIndex(averagDist)];
    double naiveScaleFact = maxPointSpecY / maxPointAveragY;
    overlapObject.scaleFact = naiveScaleFact;

    calcOverlapScore(pointsList, g_averagIsoDist, test_parameters, overlapObject);
}

/*!
 * \brief score a mono / isotope distribution where the scale factor has been set
 * \param pointsList spectrum points
 * \param stdDev uncertainty / uncertainty tolerance in ppm
 * \param overlapObject return isotope object
 */
PMI_COMMON_CORE_MINI_EXPORT  void testMonoFinal(const PlotBase & pointsList, double ppm, ChargeScoreInfo & overlapObject)
{
    TestParameters test_parameters;
    test_parameters.uncertainty_scaling_type = LinearUncertainty;
    test_parameters.uncertainty = ppm;

    calcOverlapScore(pointsList, g_averagIsoDist, test_parameters, overlapObject);
}

} // core_mini

_PMI_END

