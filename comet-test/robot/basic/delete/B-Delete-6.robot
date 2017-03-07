*** Settings ***
Documentation          Basic test case (Delete one volume with random name).
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
${TESTCASE}            B-Create-6
${WORKDIR}             /root/dev
${ID}                  0
${VOL_ID}              A-Random-Volume-ID

*** Test Cases ***
Delete volume with random name
    ${output}=    Run      bash ${WORKDIR}/comet-test/robot/basic/scripts/api.sh ${APIHOST}:${PORT} delete ${VOL_ID}
    Log to console	\n${output}
    Should Contain      ${output}      "Result": "4000"

*** Keywords ***
Open Connection And Log In
   Open Connection    ${HOST}
   Login    ${USERNAME}    ${PASSWORD}
