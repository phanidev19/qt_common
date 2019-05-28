/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#include "ComparisonCommand.h"
#include "MSCommandLineReader.h"
#include "ExtractCalibrationPointsCommand.h"
#include "ExtractMassIntensityCommand.h"
#include "MakeByspec2Command.h"
#include "MakeByspec2PmiCommand.h"
#include "NonUniformCacheCommand.h"

#include <QDebug>

MSCommandLineReader::MSCommandLineReader(MSTestCommandLineParser *parser)
    : m_parser(parser)
{
}

MSCommandLineReader::~MSCommandLineReader()
{
}

pmi::Err MSCommandLineReader::process()
{
    QScopedPointer<MSReaderCommand> command;
    pmi::Err e = pmi::kNoErr;

    switch (m_parser->commandId()) {
    case PMCommandLineParser::CommandMassIntensity:
        command.reset(new ExtractMassIntensityCommand(m_parser));
        break;

    case PMCommandLineParser::CommandCalibrationPoint:
        command.reset(new ExtractCalibrationPointsCommand(m_parser));
        break;

    case PMCommandLineParser::CommandMakeBySpec2:
        command.reset(new MakeByspec2Command(m_parser));
        break;

    case PMCommandLineParser::CommandMakeBySpec2Pmi:
        command.reset(new MakeByspec2PmiCommand(m_parser));
        break;

    case PMCommandLineParser::CommandNonUniformCache:
        command.reset(new NonUniformCacheCommand(m_parser));
        break;

    case PMCommandLineParser::CommandCompare:
        command.reset(new ComparisonCommand(m_parser));
        break;

    case PMCommandLineParser::CommandNone:
        // no --command argument in command line or "--command 0"
    case PMCommandLineParser::MaxCommand:
    default:
        qWarning() << "Invalid command ID";
        e = pmi::kBadParameterError;
    }

    if (command != nullptr) {
        e = command->execute();
    }
    return e;
}
