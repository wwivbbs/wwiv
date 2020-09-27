#!/bin/bash

# Download latest agent
wget -N https://build.wwivbbs.org/jenkins/jnlpJars/agent.jar

# start agent
java -jar agent.jar \
-jnlpUrl https://build.wwivbbs.org/jenkins/computer/[NODENAME]/slave-agent.jnlp \
-secret @secret-file \
-workDir "/jenkins" &
disown