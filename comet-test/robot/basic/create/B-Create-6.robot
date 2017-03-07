*** Settings ***
Documentation          Basic test case (Create one container with one volume alreay been used).
...

Library                SSHLibrary
Library                OperatingSystem
Suite Setup            Open Connection And Log In
Suite Teardown         Close All Connections

*** Variables ***
${APIHOST}             192.168.14.200
${PORT}                9876
${HOST}                192.168.14.41
${USERNAME}            root
${PASSWORD}            123456
${TESTCASE}            B-Create-6
${WORKDIR}             /root/dev
${ID}                  0
${VOL_ID}              ${TESTCASE}-${ID}

*** Test Cases ***
Start deploy onto host
    Log to console    ${HOST}
    ${output}=    Execute Command    echo Hello SSHLibrary!
    Should Be Equal    ${output}    Hello SSHLibrary!

Create one container with one volume
    ${output}=    Execute Command      bash ${WORKDIR}/comet-test/robot/basic/scripts/CreateOne.sh ${TESTCASE} ${ID} ${APIHOST}:${PORT}
    Log to console	\nContainer created with ID: ${output}
    Should Not Contain  ${output}    Robot-Fail

Create one container with same volume
    ${output}      ${stderr}=    Execute Command    docker run -d --name B-Create-6-Dup -v ${VOL_ID}:/data --volume-driver=codrv ubuntu:latest sleep 10s    return_stderr=True
    Log to console	\nContainer create with duplicated volume name: ${output} ${stderr}
    Should Not Be Empty	    ${stderr}
    sleep     5s
    ${output}=    Execute Command      docker rm B-Create-6-Dup

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
