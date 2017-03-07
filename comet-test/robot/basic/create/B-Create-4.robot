*** Settings ***
Documentation          Basic test case (Create one container with 5 volumes).
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
${TESTCASE}            B-Create-4
${WORKDIR}             /root/dev
${ID}                  0

*** Test Cases ***
Start deploy onto host
    Log to console    ${HOST}
    ${output}=    Execute Command    echo Hello SSHLibrary!
    Should Be Equal    ${output}    Hello SSHLibrary!

Create one container with one volume
    ${output}     ${stderr}=      Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/CreateFive.sh ${TESTCASE} ${ID} ${APIHOST}:${PORT}     return_stderr=True
    Log to console	\nContainer created with ID: ${output} ${stderr}
    Should Not Contain  ${output}    Robot-Fail

*** Keywords ***
Open Connection And Log In
   Open Connection    ${HOST}
   Login    ${USERNAME}    ${PASSWORD}
