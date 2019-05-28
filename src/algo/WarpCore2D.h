/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
 */

#ifndef __WARP_CORE_2D_H__
#define __WARP_CORE_2D_H__

#include <pmi_common_ms_export.h>
#include <common_errors.h>
#include <pmi_core_defs.h>

#include <WarpElement.h>

_PMI_BEGIN

/*!
    \brief This class is responsible for constructing a time warp. The '2D' suffix means that the
        construction of the time warp takes into account a second dimension of the input samples. While
        the 1D warp takes as input 1D signals (plots: TIC, basePeak or such) and aligns them, the 2D warp
        gets as input 2D signals and it examines more than one intensity per point in time. More than one
        intensities per time point is implemented here as WarpElements (\see WarpElement). So the inputs
        here are sequences of WarpElement (instead of sequences of intensities).
        
    Note: This is a low-level unit. It might be more practical to use \a TimeWarp2D or even 
        \a MultiSampleTimeWarp2D for high-level purposes.
*/
class PMI_COMMON_MS_EXPORT WarpCore2D
{
public:
    WarpCore2D();
    ~WarpCore2D();

    /*! \brief Gets the stretch penalty setting. */
    double stretchPenalty() const;

    /*!
        \brief Sets the stretch penalty setting.
            Use a small value (ex 0.01). This is intensity-neutral, so the same value will
            work equally well with signals of any intensity. The result will vary though, depending
            on the sampling rate used because "stretching" is calculated in index space.
    */
    void setStretchPenalty(double p);

    /*! \brief Gets global skew. */
    int globalSkew() const;

    /*! 
        \brief Sets global skew.    
            This is a search-space optimization. It will limit the search so that the maximum expected
            offset will be "global skew" samples. Use a larger value to potentially produce better warps.
    */
    Err setGlobalSkew(int s);

    /*! \brief Gets ppm value used for detecting matching mz. */
    double mzMatchPpm() const;

    /*!
        \brief Sets ppm value used for detecting matching mz.
            This only affects the result when combined with WarpElements that contain mz information. In
            such a case the score function will diff the matching pairs of mz/intensity contained in each
            WarpElement.
        \see WarpElement
    */
    void setMzMatchPpm(double ppm);

    /*!
        \brief Constructs a time warp.
        \param a Reference signal
        \param b Signal to be warped on reference.
        \param knotsA List of indices in 'a' which split the entire signal a into segments.
        \param [out] knotsB Output of the time warp. This list is populated with indices in 'b' which 
            correspond to the indices in knotsA.
    */
    Err constructWarp(std::vector<WarpElement> &a, std::vector<WarpElement> &b,
        const std::vector<int> &knotsA, std::vector<int> *knotsB) const;

private:
    double m_stretchPenalty;
    int m_globalSkew;
    double m_mzMatchPpm;
};

_PMI_END

#endif // __WARP_CORE_2D_H__