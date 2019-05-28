#ifndef WORKFLOW_LOGS_H
#define WORKFLOW_LOGS_H

#include <iostream>
#include <QString>

#define PROGRESS_TITLE "ProgressTitle"
#define PROGRESS_ERROR "Error"
#define PROGRESS_RESULT "Status"
#define PROGRESS_PERCENT "OverallProgressPercent"
#define PROGRESS_SUCCESS "Success"
#define PROGRESS_FAILED "Failed"
#define PROGRESS_INFO "ProgressInfo"
#define PROGRESS_PUSH "ProgressPush"
#define PROGRESS_POP "ProgressPop"

inline void coutTag(const QString& tag, const QString& msg) {
    std::cout << "<" << tag.toUtf8().data() << ">" << msg.toUtf8().data() << "</" << tag.toUtf8().data() << ">" << std::endl;
}

#endif // WORKFLOW_LOGS_H

