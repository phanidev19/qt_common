/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Alex Prikhodko alex.prikhodko@proteinmetrics.com
*/

#pragma once 
#include <atlbase.h>
#include <vector>
#include <limits>

//! A convenience wrapper for SAFEARRAY (No error checks yet)
template <class T> 
class SafeArrayWrapper
{
public:
    SafeArrayWrapper(SAFEARRAY* safeArray)
    {
        m_safeArray = safeArray;

        initialize();
    }

    virtual ~SafeArrayWrapper()
    {
        release();
    }

    T back() const
    {
        return value(upper());
    }

    T front() const
    {
        return value(lower());
    }

    long lower() const
    {
        return m_lower;
    }

    long upper() const
    {
        return m_upper;
    }

    inline T value(int index) const
    {
        return m_arr[index];
    }

    long count() const
    {
        if (!m_initialized) {
            return 0;
        }

        return upper() - lower() + 1;
    }

    T sum() const
    {
        if (!m_initialized) {
            return T();
        }

        T sum = T();
        const int up = upper();
        for (int i = lower(); i <= up; ++i) {
            sum += m_arr[i];
        }

        return sum;
    }

    T maximum() const
    {
        if (!m_initialized) {
            return T();
        }

        // workaround: error C2589: '(': illegal token on right side of '::'
        T maxVal = (std::numeric_limits<T>::min)();

        const int up = upper();
        for (int i = lower(); i <= up; ++i) {
            if (maxVal < m_arr[i]) {
                maxVal = m_arr[i];
            }
        }

        return maxVal;
    }

private:
    bool initialize()
    {
        HRESULT hr = SafeArrayGetLBound(m_safeArray, 1, &m_lower);
        if (FAILED(hr)) {
            return false;
        }
        hr = SafeArrayGetUBound(m_safeArray, 1, &m_upper);
        if (FAILED(hr)) {
            return false;
        }

        if (m_lower > m_upper) {
            return false;
        }

        hr = SafeArrayAccessData(m_safeArray, reinterpret_cast<void**>(&m_arr));
        m_initialized = SUCCEEDED(hr);
        return m_initialized;
    }

    void release()
    {
        if (!m_initialized) {
            return;
        }

        SafeArrayUnaccessData(m_safeArray);
        SafeArrayDestroyData(m_safeArray);
        m_safeArray = nullptr;
        m_arr = nullptr;
        m_lower = -1;
        m_upper = -1;
        m_initialized = false;
    }

private:
    SAFEARRAY* m_safeArray = nullptr;
    T* m_arr = nullptr;
    long m_lower = -1;
    long m_upper = -1;
    bool m_initialized = false;
};

template<typename T>
bool safeArrayFromVector(const std::vector<T>& src, SAFEARRAY** safeArray)
{
    SAFEARRAYBOUND bound;
    bound.lLbound = 0;
    bound.cElements = static_cast<ULONG>(src.size());

    *safeArray = SafeArrayCreate(VT_R8, 1, &bound);

    T HUGEP* ppData;
    HRESULT hr = SafeArrayAccessData(*safeArray, (void HUGEP* FAR*)&ppData);
    if (FAILED(hr)) {
        return false;
    }

    for (size_t i = 0; i < bound.cElements; i++) {
        *ppData++ = src.at(i);
    }

    SafeArrayUnaccessData(*safeArray);
    return true;
}

template<typename T>
void putElementToSafeArray(T* obj, SAFEARRAY** arr)
{
    SAFEARRAYBOUND bounds[1] = { 0 };
    bounds[0].lLbound = 0;
    bounds[0].cElements = 1;
    long index[1] = { 0 };

    *arr = SafeArrayCreate(VT_DISPATCH, 1, bounds);
    SafeArrayPutElement(*arr, index, (void*)obj);
}
