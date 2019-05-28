//load PMI-CI-Helpers from https://bitbucket.org/ykil/jenkins-shared-library.git
@Library('PMI-CI-Helpers@master') _

//abort old build if it is a feature branch
pmiAbortPreviousRunningBuilds(exclude: ['master', 'develop'])

pmiLoadParamsFromPreviousBuild(exclude_branches: ['master', 'develop'], params: ['PEL_BRANCH'])

//Dynamically allocateNode if using j2 and return node label if using j1
AGENT_LABEL = pmiAllocateNode(node : "qal_qtc")

//For some reason other projects does not have permission to copy artifact by default
properties([[$class: "CopyArtifactPermissionProperty", projectNames: "*"]])


pipeline {
    agent {
        node {
            label "${AGENT_LABEL}"
            customWorkspace "C:\\Jenkins\\workspace\\qt_common"
        }
    }
    environment {
        INSTALL_COMPONENTS = "" //install defaults components
        QTEST_FUNCTION_TIMEOUT = "600000" 
        CTEST_OUTPUT_ON_FAILURE = 1 //log output for failed tests
    }
    parameters {
        // NOTE: Automatic build will be triggered using default parameters
        // in case you have a special case, update this file and commit the change to your working branch
        string(name: "PEL_BRANCH", defaultValue: pmiBranchSelector(), description: pmiParams.desc.pel)
        string(name: "JIRA_TICKET", defaultValue: pmiParseJiraTicket(), description: pmiParams.desc.jira_ticket)
        string(name: "NOTES", defaultValue: "", description: pmiParams.desc.notes)
        booleanParam(name: "DONT_ADD_JIRA_COMMENT", defaultValue: false, description: pmiParams.desc.dont_add_jira_comment)
        booleanParam(name: "BUILD_TESTING", defaultValue: true, description: pmiParams.desc.build_testing)
        booleanParam(name: "DEBUG_SCRIPTS", defaultValue: false, description: pmiParams.desc.debug_scripts)
        booleanParam(name: "FORCE_PEL_UPDATE", defaultValue: false, description: pmiParams.desc.force_pel_update) 
        booleanParam(name: "CPP_CHECK", defaultValue: false, description: "Perform static code analysis using cppcheck")
        choice(name: "CONF", choices: pmiParams.value.conf, description: pmiParams.desc.conf)
        choice(name: "COMPILER_PRODUCT", choices: pmiParams.value.compiler_product , description: pmiParams.desc.compiler_product)
        choice(name: "ARCHITECTURE", choices: pmiParams.value.arch, description: pmiParams.desc.arch)
    }
    options {
        timestamps()
        timeout(time: 2, unit: 'HOURS')
    }
    stages {
        stage("Pre build") {
            steps {
                pmiPrintBuildInfo(params: params)

                // check if required tools are avaible on node
                pmiVerifyRequiredTools() 

                //check/fix local working branch
                pmiSetupLocalGitBranch()

                pmiCleanRepository(excludes: ['pmi_externals_libs_new'])
            }
        }
        stage("Download Test data") {
            when {
                expression { fileExists("c:\\s3") } //only in j2
            }
            steps {
                pmiDownloadS3Data(data: "autotests")     
            }
        } 
        stage("Setup PMI libs") {
            steps {
                script{
                    pmiCloneAndSetupPEL(branch: params.PEL_BRANCH, force_update: params.FORCE_PEL_UPDATE)
                }
            }
        }
        stage("Build") {
            steps {
                //Compile QTC project
                pmiBuildProject()

                //archive artifacts
                dir("build"){
                    archiveArtifacts artifacts: "log/**/*, inst/**/*, git_log_*.txt", fingerprint: true    
                }                
            }
        }
        stage("Autotest") {
            when {
                expression { params.BUILD_TESTING }
            }
            steps {
                pmiRunAutoTest() 
                pmiPublishReportTestResults()
            }
        }  
        stage("cppcheck") {
            when {
                expression { params.CPP_CHECK }
            }
            steps {
                pmiCppcheck()
            }
        }
    }//end stages
    post {
        always {
            pmiPublishJiraCommentStatus(ticket: params.JIRA_TICKET, skip: params.DONT_ADD_JIRA_COMMENT)

            //notify slack channel if build failed on (develop and PEL branch is develop) or (master and PEL branch is master)
            pmiNotifyBuildFailure(channel: '#dev-qt_common', branches: ['BRANCH_NAME', 'PEL_BRANCH'])
        }
    }
}
