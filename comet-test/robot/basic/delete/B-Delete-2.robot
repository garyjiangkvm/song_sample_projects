*** Settings ***
Documentation          Basic test case (Delete 10 volumes for B-Create-2).
...                    

Library                OperatingSystem
Library                SSHLibrary
Suite Setup            Open Connection And Log In
Suite Teardown         Close All Connections

*** Variables ***
${HOST}                192.168.14.41
${USERNAME}            root
${PASSWORD}            123456
${APIHOST}             192.168.14.200
${PORT}                9876
${TESTCASE}            B-Create-2
${WORKDIR}             /root/dev
${ID}                  0
${LOOP}                10

*** Test Cases ***
Delete 10 Volumes with API
    : FOR    ${INDEX}    IN RANGE    0      ${LOOP}
    \      ${VOL_ID}=      Catenate     SEPARATOR=-    ${TESTCASE}     ${INDEX}
    \      ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} delete ${VOL_ID}
    \      Log to console	\n${output}
    \      Should Contain      ${output}      "Result": "4002"

Start deploy onto host
    Log to console    ${HOST}
    ${output}=    Execute Command    echo Hello SSHLibrary!
    Should Be Equal    ${output}    Hello SSHLibrary!

Delete 10 volumes with docker command
    : FOR    ${INDEX}    IN RANGE    0      ${LOOP}
    \      ${output}    ${stderr}=    Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/DeleteOne.sh ${TESTCASE} ${INDEX} ${APIHOST}:${PORT}      return_stderr=True
    \      Log to console	\nContainer created with ID: ${output}    ${stderr}
    \      Should Not Contain  ${output}    Robot-Fail

Delete 10 volumes with API again
    : FOR    ${INDEX}    IN RANGE    0      ${LOOP}
    \      ${VOL_ID}=      Catenate     SEPARATOR=-    ${TESTCASE}     ${INDEX}
    \      ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} delete ${VOL_ID}
    \      Log to console	\n${output}
    \      Should Contain      ${output}      "Result": "0"

Check again with API
    : FOR    ${INDEX}    IN RANGE    0      ${LOOP}
    \      ${VOL_ID}=      Catenate     SEPARATOR=-    ${TESTCASE}     ${INDEX}
    \      ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} inspect ${VOL_ID}
    \      Log to console	\n${output}
    \      Should Contain      ${output}      "Result": "4000"

*** Keywords ***
Open Connection And Log In
   Open Connection    ${HOST}
   Login    ${USERNAME}    ${PASSWORD}
