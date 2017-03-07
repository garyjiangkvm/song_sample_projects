*** Settings ***
Documentation          Basic test case (Create 30 volumes with API and create 30 containers with these volumes).
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
${TESTCASE}            B-Create-13
${WORKDIR}             /root/dev
${LOOP}                5
${ID}                  0
${VOL_ID}              ${TESTCASE}-${ID}
${CAP}                 100

*** Test Cases ***
Create 30 volumes via API
    : FOR    ${INDEX}    IN RANGE    0     ${LOOP}
    \      ${VOL_ID}=      Catenate     SEPARATOR=-    ${TESTCASE}     ${INDEX}
    \      ${CAP} =      Evaluate      	${INDEX}*10 + 100
    \      ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} create ${VOL_ID} ${CAP}
    \      Log to console	\n${output}
    \      Should Contain      ${output}      "Result": "0"

Inspect 30 volumes via API
    : FOR    ${INDEX}    IN RANGE    0     ${LOOP}
    \      ${VOL_ID}=      Catenate     SEPARATOR=-    ${TESTCASE}     ${INDEX}
    \      ${CAP} =      Evaluate      	${INDEX}*10 + 100
    \      ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} inspect ${VOL_ID}
    \      Log to console	\n${output}
    \      Should Contain      ${output}      "Result": "0"
    \      Should Contain      ${output}      "ID": "${VOL_ID}"
    \      Should Contain      ${output}      "Capacity": "${CAP}"

Start deploy onto host
    Log to console    ${HOST}
    ${output}=    Execute Command    echo Hello SSHLibrary!
    Should Be Equal    ${output}    Hello SSHLibrary!

Create 30 containers with 30 volumes
    : FOR    ${INDEX}    IN RANGE    0      ${LOOP}
    \     ${VOL_ID}=      Catenate     SEPARATOR=-    ${TESTCASE}     ${INDEX}
    \     ${CAP} =      Evaluate      	${INDEX}*10 + 100
    \     ${output}     ${stderr}=    Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/CreateOne.sh ${TESTCASE} ${INDEX} ${APIHOST}:${PORT}     return_stderr=True
    \     Log to console	\nContainer created with ID: ${output} ${stderr}
    \     Should Not Contain  ${output}    Robot-Fail
    \     Should Contain      ${output}    ${VOL_ID}-CAP-GOOD-${CAP}

Cleanup with docker command
    : FOR    ${INDEX}    IN RANGE    0      ${LOOP}
    \     Delete One Volume With Container      ${INDEX}

Cleanup with API
    : FOR    ${INDEX}    IN RANGE    0      ${LOOP}
    \     Delete One Volume with API       ${INDEX}

*** Keywords ***
Open Connection And Log In
   Open Connection    ${HOST}
   Login    ${USERNAME}    ${PASSWORD}

Delete One Volume With Container
    [Arguments]        ${ARG1}
    ${output}    ${stderr}=    Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/DeleteOne.sh ${TESTCASE} ${ARG1} ${APIHOST}:${PORT}     return_stderr=True
    Log to console      \nContainer created with ID: ${output}    ${stderr}
    Should Not Contain  ${output}    Robot-Fail

Delete One Volume with API
    [Arguments]        ${ARG1}
    ${VOL_ID}=      Catenate     SEPARATOR=-    ${TESTCASE}     ${ARG1}
    ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} delete ${VOL_ID}
    Log to console      \n${output}
    Should Contain      ${output}      "Result": "0"
    ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} inspect ${VOL_ID}
    Log to console      \n${output}
    Should Contain      ${output}      "Result": "4000"
