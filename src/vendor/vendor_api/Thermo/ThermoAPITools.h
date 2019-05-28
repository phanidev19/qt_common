#ifndef __THERMO_API_TOOLS_H__
#define __THERMO_API_TOOLS_H__

//
// $Id: RawFileTypes.h 3631 2012-05-16 15:31:36Z chambm $
//
//
// Original author: Darren Kessner <darren@proteowizard.org>
//
// Copyright 2008 Spielberg Family Center for Applied Proteomics
//   Cedars-Sinai Medical Center, Los Angeles, California  90048
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <string_utils.h>

namespace pmi {
namespace thermo {

enum InstrumentModelType
{
    InstrumentModelType_Unknown = -1,

    // Finnigan MAT
    InstrumentModelType_MAT253,
    InstrumentModelType_MAT900XP,
    InstrumentModelType_MAT900XP_Trap,
    InstrumentModelType_MAT95XP,
    InstrumentModelType_MAT95XP_Trap,
    InstrumentModelType_SSQ_7000,
    InstrumentModelType_TSQ_7000,
    InstrumentModelType_TSQ,

    // Thermo Electron
    InstrumentModelType_Element_2,

    // Thermo Finnigan
    InstrumentModelType_Delta_Plus_Advantage,
    InstrumentModelType_Delta_Plus_XP,
    InstrumentModelType_LCQ_Advantage,
    InstrumentModelType_LCQ_Classic,
    InstrumentModelType_LCQ_Deca,
    InstrumentModelType_LCQ_Deca_XP_Plus,
    InstrumentModelType_Neptune,
    InstrumentModelType_DSQ,
    InstrumentModelType_PolarisQ,
    InstrumentModelType_Surveyor_MSQ,
    InstrumentModelType_Tempus_TOF,
    InstrumentModelType_Trace_DSQ,
    InstrumentModelType_Triton,

    // Thermo Scientific
    InstrumentModelType_LTQ,
    InstrumentModelType_LTQ_Velos,
    InstrumentModelType_LTQ_Velos_Plus,
    InstrumentModelType_LTQ_FT,
    InstrumentModelType_LTQ_FT_Ultra,
    InstrumentModelType_LTQ_Orbitrap,
    InstrumentModelType_LTQ_Orbitrap_Discovery,
    InstrumentModelType_LTQ_Orbitrap_XL,
    InstrumentModelType_LTQ_Orbitrap_Velos,
    InstrumentModelType_LTQ_Orbitrap_Elite,
    InstrumentModelType_Orbitrap_Fusion,
    InstrumentModelType_LXQ,
    InstrumentModelType_LCQ_Fleet,
    InstrumentModelType_ITQ_700,
    InstrumentModelType_ITQ_900,
    InstrumentModelType_ITQ_1100,
    InstrumentModelType_GC_Quantum,
    InstrumentModelType_LTQ_XL_ETD,
    InstrumentModelType_LTQ_Orbitrap_XL_ETD,
    InstrumentModelType_DFS,
    InstrumentModelType_DSQ_II,
    InstrumentModelType_ISQ,
    InstrumentModelType_MALDI_LTQ_XL,
    InstrumentModelType_MALDI_LTQ_Orbitrap,
    InstrumentModelType_TSQ_Quantum,
    InstrumentModelType_TSQ_Quantum_Access,
    InstrumentModelType_TSQ_Quantum_Ultra,
    InstrumentModelType_TSQ_Quantum_Ultra_AM,
    InstrumentModelType_TSQ_Vantage_Standard,
    InstrumentModelType_Element_XR,
    InstrumentModelType_Element_GD,
    InstrumentModelType_GC_IsoLink,
    InstrumentModelType_Exactive,
    InstrumentModelType_Q_Exactive,
    InstrumentModelType_Surveyor_PDA,
    InstrumentModelType_Accela_PDA,

    InstrumentModelType_Count,
};

enum MassAnalyzerType
{
    MassAnalyzerType_Unknown = -1,
    MassAnalyzerType_Linear_Ion_Trap,
    MassAnalyzerType_Quadrupole_Ion_Trap,
    MassAnalyzerType_Single_Quadrupole,
    MassAnalyzerType_Triple_Quadrupole,
    MassAnalyzerType_TOF,
    MassAnalyzerType_Orbitrap,
    MassAnalyzerType_FTICR,
    MassAnalyzerType_Magnetic_Sector,
    MassAnalyzerType_Count
};

enum ScanFilterMassAnalyzerType
{
    ScanFilterMassAnalyzerType_Unknown = -1,
    ScanFilterMassAnalyzerType_ITMS = 0,          // Ion Trap
    ScanFilterMassAnalyzerType_TQMS = 1,          // Triple Quadrupole
    ScanFilterMassAnalyzerType_SQMS = 2,          // Single Quadrupole
    ScanFilterMassAnalyzerType_TOFMS = 3,         // Time of Flight
    ScanFilterMassAnalyzerType_FTMS = 4,          // Fourier Transform
    ScanFilterMassAnalyzerType_Sector = 5,        // Magnetic Sector
    ScanFilterMassAnalyzerType_Count = 6
};

enum ControllerType
{
    Controller_None = -1,
    Controller_MS = 0,
    Controller_Analog,
    Controller_ADCard,
    Controller_PDA,
    Controller_UV
};

inline InstrumentModelType parseInstrumentModelType(const std::string& instrumentModel)
{
    std::string type = toUpper(instrumentModel);

    if (type == "MAT253")                       return InstrumentModelType_MAT253;
    else if (type == "MAT900XP")                return InstrumentModelType_MAT900XP;
    else if (type == "MAT900XP Trap")           return InstrumentModelType_MAT900XP_Trap;
    else if (type == "MAT95XP")                 return InstrumentModelType_MAT95XP;
    else if (type == "MAT95XP Trap")            return InstrumentModelType_MAT95XP_Trap;
    else if (type == "SSQ 7000")                return InstrumentModelType_SSQ_7000;
    else if (type == "TSQ 7000")                return InstrumentModelType_TSQ_7000;
    else if (type == "TSQ")                     return InstrumentModelType_TSQ;
    else if (type == "ELEMENT2" ||
             type == "ELEMENT 2")               return InstrumentModelType_Element_2;
    else if (type == "DELTA PLUSADVANTAGE")     return InstrumentModelType_Delta_Plus_Advantage;
    else if (type == "DELTAPLUSXP")             return InstrumentModelType_Delta_Plus_XP;
    else if (type == "LCQ ADVANTAGE")           return InstrumentModelType_LCQ_Advantage;
    else if (type == "LCQ CLASSIC")             return InstrumentModelType_LCQ_Classic;
    else if (type == "LCQ DECA")                return InstrumentModelType_LCQ_Deca;
    else if (type == "LCQ DECA XP" ||
             type == "LCQ DECA XP PLUS")        return InstrumentModelType_LCQ_Deca_XP_Plus;
    else if (type == "NEPTUNE")                 return InstrumentModelType_Neptune;
    else if (type == "DSQ")                     return InstrumentModelType_DSQ;
    else if (type == "POLARISQ")                return InstrumentModelType_PolarisQ;
    else if (type == "SURVEYOR MSQ")            return InstrumentModelType_Surveyor_MSQ;
    else if (type == "TEMPUS TOF")              return InstrumentModelType_Tempus_TOF;
    else if (type == "TRACE DSQ")               return InstrumentModelType_Trace_DSQ;
    else if (type == "TRITON")                  return InstrumentModelType_Triton;
    else if (type == "LTQ" || type == "LTQ XL") return InstrumentModelType_LTQ;
    else if (type == "LTQ FT")                  return InstrumentModelType_LTQ_FT;
    else if (type == "LTQ FT ULTRA")            return InstrumentModelType_LTQ_FT_Ultra;
    else if (type == "LTQ ORBITRAP")            return InstrumentModelType_LTQ_Orbitrap;
    else if (type == "LTQ ORBITRAP DISCOVERY")  return InstrumentModelType_LTQ_Orbitrap_Discovery;
    else if (type == "LTQ ORBITRAP XL")         return InstrumentModelType_LTQ_Orbitrap_XL;
    else if (contains(type, "ORBITRAP VELOS")) return InstrumentModelType_LTQ_Orbitrap_Velos;
    else if (contains(type, "ORBITRAP ELITE")) return InstrumentModelType_LTQ_Orbitrap_Elite;
    else if (contains(type, "FUSION"))  return InstrumentModelType_Orbitrap_Fusion;
    else if (contains(type, "VELOS PLUS")) return InstrumentModelType_LTQ_Velos_Plus;
    else if (contains(type, "VELOS PRO"))  return InstrumentModelType_LTQ_Velos_Plus;
    else if (type == "LTQ VELOS")               return InstrumentModelType_LTQ_Velos;
    else if (type == "LXQ")                     return InstrumentModelType_LXQ;
    else if (type == "LCQ FLEET")               return InstrumentModelType_LCQ_Fleet;
    else if (type == "ITQ 700")                 return InstrumentModelType_ITQ_700;
    else if (type == "ITQ 900")                 return InstrumentModelType_ITQ_900;
    else if (type == "ITQ 1100")                return InstrumentModelType_ITQ_1100;
    else if (type == "GC QUANTUM")              return InstrumentModelType_GC_Quantum;
    else if (type == "LTQ XL ETD")              return InstrumentModelType_LTQ_XL_ETD;
    else if (type == "LTQ ORBITRAP XL ETD")     return InstrumentModelType_LTQ_Orbitrap_XL_ETD;
    else if (type == "DFS")                     return InstrumentModelType_DFS;
    else if (type == "DSQ II")                  return InstrumentModelType_DSQ_II;
    else if (type == "ISQ")                     return InstrumentModelType_ISQ;
    else if (type == "MALDI LTQ XL")            return InstrumentModelType_MALDI_LTQ_XL;
    else if (type == "MALDI LTQ ORBITRAP")      return InstrumentModelType_MALDI_LTQ_Orbitrap;
    else if (type == "TSQ QUANTUM")             return InstrumentModelType_TSQ_Quantum;
    else if (type == "TSQ QUANTUM ACCESS")      return InstrumentModelType_TSQ_Quantum_Access;
    else if (type == "TSQ QUANTUM ULTRA")       return InstrumentModelType_TSQ_Quantum_Ultra;
    else if (type == "TSQ QUANTUM ULTRA AM")    return InstrumentModelType_TSQ_Quantum_Ultra_AM;
    else if (type == "TSQ VANTAGE STANDARD")    return InstrumentModelType_TSQ_Vantage_Standard;
    else if (type == "ELEMENT XR")              return InstrumentModelType_Element_XR;
    else if (type == "ELEMENT GD")              return InstrumentModelType_Element_GD;
    else if (type == "GC ISOLINK")              return InstrumentModelType_GC_IsoLink;
    else if (contains(type, "Q EXACTIVE")) return InstrumentModelType_Q_Exactive;
    else if (contains(type, "EXACTIVE"))   return InstrumentModelType_Exactive;
    else if (type == "SURVEYOR PDA")            return InstrumentModelType_Surveyor_PDA;
    else if (type == "ACCELA PDA")              return InstrumentModelType_Accela_PDA;
    else
        return InstrumentModelType_Unknown;
}

inline MassAnalyzerType
convertScanFilterMassAnalyzer(ScanFilterMassAnalyzerType scanFilterType,
                              InstrumentModelType instrumentModel)
{
    switch (instrumentModel)
    {
        case InstrumentModelType_Exactive:
        case InstrumentModelType_Q_Exactive:
            return MassAnalyzerType_Orbitrap;

        case InstrumentModelType_LTQ_Orbitrap:
        case InstrumentModelType_LTQ_Orbitrap_Discovery:
        case InstrumentModelType_LTQ_Orbitrap_XL:
        case InstrumentModelType_LTQ_Orbitrap_XL_ETD:
        case InstrumentModelType_MALDI_LTQ_Orbitrap:
        case InstrumentModelType_LTQ_Orbitrap_Velos:
        case InstrumentModelType_LTQ_Orbitrap_Elite:
        case InstrumentModelType_Orbitrap_Fusion:
            if (scanFilterType == ScanFilterMassAnalyzerType_FTMS)
                return MassAnalyzerType_Orbitrap;
            else
                return MassAnalyzerType_Linear_Ion_Trap;

        case InstrumentModelType_LTQ_FT:
        case InstrumentModelType_LTQ_FT_Ultra:
            if (scanFilterType == ScanFilterMassAnalyzerType_FTMS)
                return MassAnalyzerType_FTICR;
            else
                return MassAnalyzerType_Linear_Ion_Trap;

        case InstrumentModelType_SSQ_7000:
        case InstrumentModelType_Surveyor_MSQ:
        case InstrumentModelType_DSQ:
        case InstrumentModelType_DSQ_II:
        case InstrumentModelType_ISQ:
        case InstrumentModelType_Trace_DSQ:
        case InstrumentModelType_GC_IsoLink:
            return MassAnalyzerType_Single_Quadrupole;

        case InstrumentModelType_TSQ_7000:
        case InstrumentModelType_TSQ:
        case InstrumentModelType_TSQ_Quantum:
        case InstrumentModelType_TSQ_Quantum_Access:
        case InstrumentModelType_TSQ_Quantum_Ultra:
        case InstrumentModelType_TSQ_Quantum_Ultra_AM:
        case InstrumentModelType_TSQ_Vantage_Standard:
        case InstrumentModelType_GC_Quantum:
            return MassAnalyzerType_Triple_Quadrupole;

        case InstrumentModelType_LCQ_Advantage:
        case InstrumentModelType_LCQ_Classic:
        case InstrumentModelType_LCQ_Deca:
        case InstrumentModelType_LCQ_Deca_XP_Plus:
        case InstrumentModelType_LCQ_Fleet:
        case InstrumentModelType_PolarisQ:
        case InstrumentModelType_ITQ_700:
        case InstrumentModelType_ITQ_900:
            return MassAnalyzerType_Quadrupole_Ion_Trap;

        case InstrumentModelType_LTQ:
        case InstrumentModelType_LXQ:
        case InstrumentModelType_LTQ_XL_ETD:
        case InstrumentModelType_ITQ_1100:
        case InstrumentModelType_MALDI_LTQ_XL:
        case InstrumentModelType_LTQ_Velos:
        case InstrumentModelType_LTQ_Velos_Plus:
            return MassAnalyzerType_Linear_Ion_Trap;

        case InstrumentModelType_DFS:
        case InstrumentModelType_MAT253:
        case InstrumentModelType_MAT900XP:
        case InstrumentModelType_MAT900XP_Trap:
        case InstrumentModelType_MAT95XP:
        case InstrumentModelType_MAT95XP_Trap:
            return MassAnalyzerType_Magnetic_Sector;

        case InstrumentModelType_Tempus_TOF:
            return MassAnalyzerType_TOF;

        case InstrumentModelType_Element_XR:
        case InstrumentModelType_Element_2:
        case InstrumentModelType_Element_GD:
        case InstrumentModelType_Delta_Plus_Advantage:
        case InstrumentModelType_Delta_Plus_XP:
        case InstrumentModelType_Neptune:
        case InstrumentModelType_Triton:
            // TODO: get mass analyzer information for these instruments
            return MassAnalyzerType_Unknown;

        case InstrumentModelType_Surveyor_PDA:
        case InstrumentModelType_Accela_PDA:
        case InstrumentModelType_Unknown:
        default:
            return MassAnalyzerType_Unknown;
    }
    return MassAnalyzerType_Unknown;
}

} //thermo
} //pmi

#endif

