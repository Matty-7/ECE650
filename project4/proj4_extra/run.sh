#!/bin/bash

if [ ! -d "orm_env" ]; then
    echo "Please run ./setup.sh to set up the environment first"
    exit 1
fi

source orm_env/bin/activate

python main.py

deactivate
