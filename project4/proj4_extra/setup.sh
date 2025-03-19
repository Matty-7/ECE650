#!/bin/bash

python3 -m venv orm_env

source orm_env/bin/activate

pip install -r requirements.txt

echo "Environment setup complete! Use source orm_env/bin/activate to activate the environment"
