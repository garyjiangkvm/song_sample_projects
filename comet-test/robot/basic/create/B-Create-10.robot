*** Settings ***
Documentation          Basic test case (Create one volume with API and Delete it).
...                    

Library                OperatingSystem
Suite Setup            
Suite Teardown         

*** Variables ***
${APIHOST}             192.168.14.200
${PORT}                9876
${TESTCASE}            B-Create-10
${WORKDIR}             /root/dev
${ID}                  0
${VOL_ID}              ${TESTCASE}-${ID}
${CAP}                 100

*** Test Cases ***
Create one volume via API
    ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} create ${VOL_ID} ${CAP}
    Log to console	\n${output}
    Should Contain      ${output}      "Result": "0"

Inspect one volume via API
    ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} inspect ${VOL_ID}
    Log to console	\n${output}
    Should Contain      ${output}      "Result": "0"
    Should Contain      ${output}      "ID": "${VOL_ID}"
    Should Contain      ${output}      "Capacity": "${CAP}"

Cleanup
    Delete Volume with API      ${VOL_ID}

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
