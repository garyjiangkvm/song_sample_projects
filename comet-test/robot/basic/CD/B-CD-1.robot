*** Settings ***
Documentation          Basic test case (Repeat to Create one container and delete it 1000 times).
...

Library                SSHLibrary
Library                OperatingSystem
Suite Setup            Open Connection And Log In
Suite Teardown         Close All Connections

*** Variables ***
${APIHOST}             192.168.14.200
${PORT}                9876
${HOST}                192.168.14.50
${USERNAME}            root
${PASSWORD}            123456
${LOOP}                3
${TESTCASE}            B-CD-1
${WORKDIR}             /root/dev
${ID}                  0
${VOL_ID}              ${TESTCASE}-${ID}

*** Test Cases ***
Start deploy onto host
    Log to console    ${HOST}
    ${output}=    Execute Command    echo Hello SSHLibrary!
    Should Be Equal    ${output}    Hello SSHLibrary!

Create & Delete one container with one volume
    : FOR    ${INDEX}    IN RANGE    0     ${LOOP}
    \      ${output}    ${stderr}=    Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/CreateOne.sh ${TESTCASE} ${INDEX} ${APIHOST}:${PORT}     return_stderr=True
    \      Log to console	\nContainer created with ID: ${output} ${stderr}
    \      Should Not Contain  ${output}    Robot-Fail
    \      Sleep      1s
    \      ${output}    ${stderr}=    Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/DeleteOne.sh ${TESTCASE} ${INDEX} ${APIHOST}:${PORT}     return_stderr=True
    \      Log to console	\nContainer created with ID: ${output} ${stderr}
    \      Should Not Contain  ${output}    Robot-Fail

Cleanup
    : FOR    ${INDEX}    IN RANGE    0     ${LOOP}
    \      ${VOL_ID}=      Catenate     SEPARATOR=-    ${TESTCASE}     ${INDEX}
    \      Delete Volume with API      ${VOL_ID}

*** Keywords ***
Open Connection And Log In
   Open Connection    ${HOST}
   Login    ${USERNAME}    ${PASSWORD}

Delete Volume with API
    [Arguments]        ${ARG1}
    ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} delete ${ARG1}
    Log to console	\n${output}
    Should Contain      ${output}      "Result": "0"
    ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} inspect ${ARG1}
    Log to console	\n${output}
    Should Contain      ${output}      "Result": "4000"
