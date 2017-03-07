*** Settings ***
Documentation          Basic test case (Docker migration with volume).
...

Library                SSHLibrary
Library                OperatingSystem
Library                ../scripts/MyLibrary.py
Suite Setup            
Suite Teardown         Close All Connections

*** Variables ***
${APIHOST}             192.168.14.200
${PORT}                9876
${ALL}                 192.168.14.40+192.168.14.41+192.168.14.50
${HOST}                192.168.14.41
${USERNAME}            root
${PASSWORD}            123456
${TESTCASE}            B-Migrate-1
${WORKDIR}             /root/dev
${ID}                  0
${VOL_ID}              ${TESTCASE}-${ID}

*** Test Cases ***
Get Hosts
    @{HOSTS} =      Get Hosts        ${ALL}     ${HOST}
    Log to console	\n@{HOSTS}
    Set Suite Variable    @{HOSTS}
    
Start connect to hosts
    : FOR    ${H}    IN    @{HOSTS}
    \      Log to console	\nLogin to ${H}
    \      Open Connection        ${H}      alias=${H}
    \      Login    ${USERNAME}    ${PASSWORD}
    \      ${output}=    Execute Command    echo Hello SSHLibrary!
    \      Should Be Equal    ${output}    Hello SSHLibrary!

Start deploy onto host
    : FOR    ${H}    IN    @{HOSTS}
    \      Switch Connection      ${H}
    \      Log to console	\nSwitch to ${H}
    \      ${output}    ${stderr}=    Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/CreateOne.sh ${TESTCASE} ${ID} ${APIHOST}:${PORT} ${APIHOST}     return_stderr=True
    \      Log to console	\nContainer created with ID: ${output} ${stderr}
    \      Should Not Contain  ${output}    Robot-Fail
    \      Sleep      5s
    \      ${output}    ${stderr}=    Execute Command      python ${WORKDIR}/comet-test/robot/basic/scripts/check-vol.py ${TESTCASE}-${ID} 100     return_stderr=True
    \      Log to console	\n${output} ${stderr}
    \      Should Not Contain  ${output}    Robot-Fail
    \      ${output}    ${stderr}=    Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/DeleteOne.sh ${TESTCASE} ${ID} ${APIHOST}:${PORT}     return_stderr=True
    \      Log to console	\nContainer created with ID: ${output} ${stderr}
    \      Should Not Contain  ${output}    Robot-Fail

Cleanup volume
    ${VOL_ID}=      Catenate     SEPARATOR=-    ${TESTCASE}     ${ID}
    Delete Volume with API      ${VOL_ID}

Cleanup etcd key
    ${output}=     Run      curl -s http://${APIHOST}:2379/v2/keys/${VOL_ID} -XDELETE
    Log to console	\n${output}
    Should Contain  ${output}    "action":"delete"

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
