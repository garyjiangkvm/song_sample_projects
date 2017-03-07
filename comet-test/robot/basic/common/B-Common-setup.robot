*** Settings ***
Documentation          Setup Basic Test on a remote machine.i.e Copy files
...                    and getting their output and the return code.
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
${TESTCASE}            B-Common-setup
${WORKDIR}             /root/dev

*** Test Cases ***
Start deploy driver onto host
    Log to console    ${HOST}

Copy directory
    [Documentation]    Copy all files on the remote machine.
    Execute Command    rm -rf ${WORKDIR}/comet-test
    Put Directory      ${WORKDIR}/comet-test      ${WORKDIR}/comet-test    recursive=True
    Directory Should Exist    ${WORKDIR}/comet-test
    ${output}=    Execute Command    echo ${HOST} > ${WORKDIR}/comet-test/host-ip

*** Keywords ***
Open Connection And Log In
   Open Connection    ${HOST}
   Login    ${USERNAME}    ${PASSWORD}
