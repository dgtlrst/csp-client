#!/usr/bin/bash

GIT_HOOKS_FOLDER=./.git/hooks

echo "installing git hooks to $GIT_HOOKS_FOLDER..."
cp -f .githooks/* $GIT_HOOKS_FOLDER/