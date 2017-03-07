*** Settings ***
Documentation          Basic test case (Delete 5 volumes for B-Create-4).
...                    

Library                OperatingSystem
Library                SSHLibrary
Suite Setup            
Suite Teardown         

*** Variables ***
${HOST}                192.168.14.41
${USERNAME}            root
${PASSWORD}            123456
${APIHOST}             192.168.14.200
${PORT}                9876
${TESTCASE}            B-Create-5
${WORKDIR}             /root/dev
${ID}                  0
${LOOP}                10

*** Test Cases ***
Delete 50 Volumes with API
    : FOR    ${INDEX}    IN RANGE    0      ${LOOP}
    \      Inner Loop1      ${INDEX}

*** Keywords ***
Open Connection And Log In
   Open Connection    ${HOST}
   Login    ${USERNAME}    ${PASSWORD}

Inner Loop1 
    [Arguments]        ${Arg1}
    : FOR    ${I}    IN RANGE    0      5
    \      ${VOL_ID}=      Catenate     SEPARATOR=-    ${TESTCASE}-${ARG1}     ${I}
    \      ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} delete ${VOL_ID}
    \      Log to console	\n${output}
