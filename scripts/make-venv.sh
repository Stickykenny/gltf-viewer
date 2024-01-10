mkdir -p output-images/.venv
python -m venv output-images/.venv
[[ -f output-images/.venv/Scripts/activate ]] && source output-images/.venv/Scripts/activate
[[ -f output-images/.venv/bin/activate ]] && source output-images/.venv/bin/activate
python -m pip install imutils opencv-python
