*** Settings ***
Documentation          Basic test case (Create one container with one volume with API).
...                    

Library                OperatingSystem
Library                SSHLibrary
Suite Setup            Open Connection And Log In
Suite Teardown         Close All Connections

*** Variables ***
${APIHOST}             192.168.14.200
${PORT}                9876
${HOST}                192.168.14.41
${USERNAME}            root
${PASSWORD}            123456
${TESTCASE}            B-Create-11
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

Start deploy onto host
    Log to console    ${HOST}
    ${output}=    Execute Command    echo Hello SSHLibrary!
    Should Be Equal    ${output}    Hello SSHLibrary!

Create one container with one volume
    ${output}     ${stderr}=    Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/CreateOne.sh ${TESTCASE} ${ID} ${APIHOST}:${PORT}     return_stderr=True
    Log to console	\nContainer created with ID: ${output}     ${stderr}
    Should Not Contain  ${output}    Robot-Fail
    Should Contain      ${output}    ${VOL_ID}-CAP-GOOD-${CAP}

Cleanup
    Delete One Volume With Container

*** Keywords ***
Open Connection And Log In
   Open Connection    ${HOST}
   Login    ${USERNAME}    ${PASSWORD}

Delete One Volume With Container
    ${output}    ${stderr}=    Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/DeleteOne.sh ${TESTCASE} ${ID} ${APIHOST}:${PORT}     return_stderr=True
    Log to console      \nContainer created with ID: ${output}    ${stderr}
    Should Not Contain  ${output}    Robot-Fail
    ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} delete ${VOL_ID}
    Log to console      \n${output}
    Should Contain      ${output}      "Result": "0"
    ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} inspect ${VOL_ID}
    Log to console      \n${output}
    Should Contain      ${output}      "Result": "4000"
