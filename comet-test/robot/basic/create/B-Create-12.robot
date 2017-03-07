*** Settings ***
Documentation          Basic test case (Create 30 volumes with API).
...                    

Library                OperatingSystem
Suite Setup            
Suite Teardown         

*** Variables ***
${APIHOST}             192.168.14.200
${PORT}                9876
${TESTCASE}            B-Create-12
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
