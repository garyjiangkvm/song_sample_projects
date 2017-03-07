*** Settings ***
Documentation          Basic test case (Create 10 containers with 10 volumes).
...

Library                SSHLibrary
Suite Setup            Open Connection And Log In
Suite Teardown         Close All Connections

*** Variables ***
${APIHOST}             192.168.14.200
${PORT}                9876
${HOST}                192.168.14.41
${USERNAME}            root
${PASSWORD}            123456
${TESTCASE}            B-Create-2
${WORKDIR}             /root/dev
${ID}                  0
${LOOP}                10

*** Test Cases ***
Start deploy onto host
    Log to console    ${HOST}
    ${output}=    Execute Command    echo Hello SSHLibrary!
    Should Be Equal    ${output}    Hello SSHLibrary!

Create 10 containers with 10 volumes
    : FOR    ${INDEX}    IN RANGE    0      ${LOOP}
    \     ${output}    ${stderr}=    Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/CreateOne.sh ${TESTCASE} ${INDEX} ${APIHOST}:${PORT}      return_stderr=True
    \     Log to console	\nContainer created with ID: ${output} ${stderr}
    \     Should Not Contain  ${output}    Robot-Fail

*** Keywords ***
Open Connection And Log In
   Open Connection    ${HOST}
   Login    ${USERNAME}    ${PASSWORD}
